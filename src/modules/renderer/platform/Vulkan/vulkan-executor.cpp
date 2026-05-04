#include "vulkan-executor.hpp"
#include "assert.hpp"
#include "log.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "shader-lang/compiler.hpp"
#include "shader-lang/layout-merge.hpp"
#include "shader-lang/pipeline-layout-serializer.hpp"
#include "shaderc-compiler.hpp"
#include "trace.hpp"
#include "virtual-texture2D.hpp"
#include "virtual-texture3D.hpp"
#include "vulkan-shader-program.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <variant>

namespace astralix {

static constexpr VkDeviceSize UPLOAD_ARENA_SIZE = 128 * 1024 * 1024;

namespace {

#ifdef ASTRA_TRACE
std::string pass_execute_trace_name(const CompiledPass &pass) {
  return pass.debug_name.empty() ? "RenderPass::execute"
                                 : pass.debug_name + "::execute";
}
#endif

uint32_t align_up_u32(uint32_t offset, uint32_t alignment) {
  return (offset + alignment - 1u) & ~(alignment - 1u);
}

uint32_t std140_base_alignment(TokenKind kind) {
  switch (kind) {
    case TokenKind::TypeBool:
    case TokenKind::TypeInt:
    case TokenKind::TypeFloat:
      return 4;
    case TokenKind::TypeVec2:
      return 8;
    case TokenKind::TypeVec3:
    case TokenKind::TypeVec4:
    case TokenKind::TypeMat3:
    case TokenKind::TypeMat4:
      return 16;
    default:
      return 4;
  }
}

bool cached_layout_has_valid_loose_global_ids(
    const ShaderPipelineLayout &layout
) {
  const auto globals_block_is_valid =
      [](const ShaderValueBlockDesc &block) -> bool {
    if (block.logical_name != "__globals") {
      return true;
    }

    for (const auto &field : block.fields) {
      const std::string expected_logical_name =
          block.logical_name + "." + field.logical_name;
      if (field.binding_id != shader_binding_id(expected_logical_name)) {
        return false;
      }
    }

    return true;
  };

  for (const auto &block : layout.resource_layout.value_blocks) {
    if (!globals_block_is_valid(block)) {
      return false;
    }
  }

  for (const auto &block : layout.pipeline_layout.value_blocks) {
    if (!globals_block_is_valid(block)) {
      return false;
    }
  }

  return true;
}

bool cached_layout_has_valid_std140_block_sizes(
    const ShaderPipelineLayout &layout
) {
  const auto block_has_valid_size =
      [](const ShaderValueBlockDesc &block) -> bool {
    if (block.fields.empty()) {
      return true;
    }

    uint32_t required_size = 0;
    for (const auto &field : block.fields) {
      required_size = std::max(required_size, field.offset + field.size);
    }

    return block.size == align_up_u32(required_size, 16u);
  };

  for (const auto &block : layout.resource_layout.value_blocks) {
    if (!block_has_valid_size(block)) {
      return false;
    }
  }

  for (const auto &block : layout.pipeline_layout.value_blocks) {
    if (!block_has_valid_size(block)) {
      return false;
    }
  }

  return true;
}

bool cached_layout_has_valid_loose_global_offsets(
    const ShaderPipelineLayout &layout
) {
  const auto globals_block_has_valid_offsets =
      [](const ShaderValueBlockDesc &block) -> bool {
    if (block.logical_name != "__globals" || block.fields.empty()) {
      return true;
    }

    for (const auto &field : block.fields) {
      if (field.logical_name.find('.') != std::string::npos) {
        return true;
      }
    }

    uint32_t current_offset = 0;
    for (const auto &field : block.fields) {
      const uint32_t alignment =
          field.array_stride > 0 ? 16u : std140_base_alignment(field.type.kind);
      current_offset = align_up_u32(current_offset, alignment);
      if (field.offset != current_offset) {
        return false;
      }

      current_offset += field.array_stride > 0 ? field.array_stride : field.size;
    }

    return block.size == align_up_u32(current_offset, 16u);
  };

  for (const auto &block : layout.resource_layout.value_blocks) {
    if (!globals_block_has_valid_offsets(block)) {
      return false;
    }
  }

  for (const auto &block : layout.pipeline_layout.value_blocks) {
    if (!globals_block_has_valid_offsets(block)) {
      return false;
    }
  }

  return true;
}

constexpr std::pair<StageKind, const char *> spirv_stage_extensions[] = {
    {StageKind::Vertex, "vert"},
    {StageKind::Fragment, "frag"},
    {StageKind::Geometry, "geom"},
    {StageKind::Compute, "comp"},
};

constexpr uint32_t k_vulkan_shader_cache_format_version = 4;

std::filesystem::path spirv_cache_path(
    const std::filesystem::path &source, const char *stage_extension
) {
  return source.parent_path() /
         (source.stem().string() + "." + stage_extension + ".spv");
}

std::filesystem::path vulkan_glsl_cache_path(
    const std::filesystem::path &source, const char *stage_extension
) {
  return source.parent_path() /
         (source.stem().string() + ".vulkan." + stage_extension + ".glsl");
}

std::filesystem::path shader_dependency_sidecar_path(
    const std::filesystem::path &source
) {
  return source.parent_path() / (source.stem().string() + ".deps");
}

std::filesystem::path shader_cache_version_sidecar_path(
    const std::filesystem::path &source
) {
  return source.parent_path() / (source.stem().string() + ".cache-version");
}

bool logical_name_ends_with(std::string_view logical_name, std::string_view suffix) {
  return logical_name.size() >= suffix.size() &&
         logical_name.compare(logical_name.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool resource_uses_black_default(std::string_view logical_name) {
  return logical_name_ends_with(logical_name, ".specular") ||
         logical_name_ends_with(logical_name, "specular") ||
         logical_name_ends_with(logical_name, "displacement_map");
}

bool resource_uses_flat_normal_default(std::string_view logical_name) {
  return logical_name_ends_with(logical_name, "normal_map");
}

std::optional<std::vector<std::filesystem::path>> read_shader_dependencies(
    const std::filesystem::path &source
) {
  const auto dependency_path = shader_dependency_sidecar_path(source);
  if (!std::filesystem::exists(dependency_path)) {
    return std::nullopt;
  }

  std::ifstream dependency_file(dependency_path);
  if (!dependency_file) {
    return std::nullopt;
  }

  std::vector<std::filesystem::path> dependencies;
  std::string line;
  while (std::getline(dependency_file, line)) {
    if (line.empty()) {
      continue;
    }

    std::filesystem::path dependency(line);
    if (dependency.is_relative()) {
      dependency = (source.parent_path() / dependency).lexically_normal();
    }
    dependencies.push_back(std::move(dependency));
  }

  return dependencies;
}

std::optional<uint32_t> read_shader_cache_version(
    const std::filesystem::path &source
) {
  const auto version_path = shader_cache_version_sidecar_path(source);
  if (!std::filesystem::exists(version_path)) {
    return std::nullopt;
  }

  std::ifstream version_file(version_path);
  if (!version_file) {
    return std::nullopt;
  }

  uint32_t version = 0;
  version_file >> version;
  if (!version_file) {
    return std::nullopt;
  }

  return version;
}

const BufferLayout *compiled_buffer_layout(const CompiledBuffer &buffer) {
  const BufferLayout &layout =
      buffer.is_transient ? buffer.transient_layout
                          : buffer.persistent_vertex_layout;
  return layout.get_elements().empty() ? nullptr : &layout;
}

void write_shader_dependencies(
    const std::filesystem::path &source,
    const std::vector<std::filesystem::path> &dependencies
) {
  std::ofstream dependency_out(
      shader_dependency_sidecar_path(source), std::ios::trunc
  );
  for (const auto &dependency : dependencies) {
    dependency_out << dependency.generic_string() << '\n';
  }
}

void write_shader_cache_version(
    const std::filesystem::path &source, uint32_t version
) {
  std::ofstream version_out(
      shader_cache_version_sidecar_path(source), std::ios::trunc
  );
  version_out << version << '\n';
}

std::optional<std::filesystem::file_time_type> newest_shader_input_mtime(
    const std::filesystem::path &source,
    const std::vector<std::filesystem::path> &dependencies
) {
  if (!std::filesystem::exists(source)) {
    return std::nullopt;
  }

  auto newest = std::filesystem::last_write_time(source);
  for (const auto &dependency : dependencies) {
    if (!std::filesystem::exists(dependency)) {
      return std::nullopt;
    }
    newest = std::max(newest, std::filesystem::last_write_time(dependency));
  }

  return newest;
}

CompileResult compile_shader_file(
    const std::filesystem::path &path,
    Compiler *shared_layout_compiler = nullptr
) {
  ASTRA_ENSURE(
      path.extension() != ".axsl",
      "[Vulkan] Shader program compilation currently requires .axsl input: ",
      path.string()
  );

  const bool can_use_file_cache = shared_layout_compiler == nullptr;

  auto layout_path = shader_layout_sidecar_path(path);
  auto cached_dependencies =
      can_use_file_cache ? read_shader_dependencies(path) : std::nullopt;
  auto cached_version =
      can_use_file_cache ? read_shader_cache_version(path) : std::nullopt;
  auto latest_input_mtime =
      cached_dependencies.has_value()
          ? newest_shader_input_mtime(path, *cached_dependencies)
          : std::nullopt;

  bool all_cached = latest_input_mtime.has_value() &&
                    cached_version.has_value() &&
                    *cached_version == k_vulkan_shader_cache_format_version &&
                    std::filesystem::exists(layout_path) &&
                    std::filesystem::last_write_time(layout_path) >=
                        *latest_input_mtime;

  bool any_spv_found = false;
  bool any_glsl_found = false;
  if (all_cached) {
    for (auto [kind, extension] : spirv_stage_extensions) {
      auto cached = spirv_cache_path(path, extension);
      if (!std::filesystem::exists(cached))
        continue;
      any_spv_found = true;
      if (latest_input_mtime.has_value() &&
          std::filesystem::last_write_time(cached) < *latest_input_mtime) {
        all_cached = false;
        break;
      }

      auto glsl_cached = vulkan_glsl_cache_path(path, extension);
      if (!std::filesystem::exists(glsl_cached)) {
        all_cached = false;
        break;
      }
      any_glsl_found = true;
      if (latest_input_mtime.has_value() &&
          std::filesystem::last_write_time(glsl_cached) < *latest_input_mtime) {
        all_cached = false;
        break;
      }
    }
    if (!any_spv_found || !any_glsl_found)
      all_cached = false;
  }

  if (all_cached) {
    CompileResult cached_result;
    cached_result.dependencies = std::move(*cached_dependencies);

    for (auto [kind, extension] : spirv_stage_extensions) {
      auto cached = spirv_cache_path(path, extension);
      if (!std::filesystem::exists(cached))
        continue;

      std::ifstream spv_file(cached, std::ios::binary | std::ios::ate);
      if (!spv_file)
        continue;

      auto byte_size = static_cast<std::streamsize>(spv_file.tellg());
      spv_file.seekg(0);

      std::vector<uint32_t> spirv(
          static_cast<size_t>(byte_size) / sizeof(uint32_t)
      );
      spv_file.read(
          reinterpret_cast<char *>(spirv.data()),
          byte_size
      );
      cached_result.spirv_stages[kind] = std::move(spirv);
    }

    std::ifstream layout_file(layout_path);
    if (layout_file) {
      std::ostringstream layout_buffer;
      layout_buffer << layout_file.rdbuf();
      std::string layout_error;
      auto layout = deserialize_shader_pipeline_layout(
          layout_buffer.str(), SerializationFormat::Json, &layout_error
      );
      if (layout.has_value() &&
          cached_layout_has_valid_loose_global_ids(*layout) &&
          cached_layout_has_valid_std140_block_sizes(*layout) &&
          cached_layout_has_valid_loose_global_offsets(*layout)) {
        cached_result.merged_layout = std::move(*layout);
        return cached_result;
      }
    }
  }

  std::ifstream file(path);
  ASTRA_ENSURE(!file, "[Vulkan] Failed to open shader source: ", path.string());

  std::ostringstream buffer;
  buffer << file.rdbuf();

  Compiler local_compiler;
  Compiler &compiler =
      shared_layout_compiler != nullptr ? *shared_layout_compiler
                                        : local_compiler;
  CompileResult result;
  if (shared_layout_compiler != nullptr) {
    result = compiler.compile_with_shared_layout_state(
        buffer.str(),
        path.parent_path().string(),
        path.string(),
        {.emit_vulkan_glsl = true}
    );
  } else {
    result = compiler.compile(
        buffer.str(),
        path.parent_path().string(),
        path.string(),
        {.emit_vulkan_glsl = true}
    );
  }

  if (!result.ok())
    return result;

  if (can_use_file_cache) {
    for (auto [kind, extension] : spirv_stage_extensions) {
      auto it = result.vulkan_glsl_stages.find(kind);
      if (it == result.vulkan_glsl_stages.end())
        continue;

      std::ofstream glsl_file(
          vulkan_glsl_cache_path(path, extension), std::ios::trunc
      );
      glsl_file << it->second;
    }
  }

  for (const auto &[stage, glsl] : result.vulkan_glsl_stages) {
    auto spirv_result = compile_glsl_to_spirv(glsl, stage, path.string());
    if (!spirv_result.ok()) {
      for (auto &error : spirv_result.errors)
        result.errors.push_back(std::move(error));
      return result;
    }
    result.spirv_stages[stage] = std::move(spirv_result.spirv);
  }

  if (can_use_file_cache) {
    for (auto [kind, extension] : spirv_stage_extensions) {
      auto it = result.spirv_stages.find(kind);
      if (it == result.spirv_stages.end())
        continue;
      std::ofstream spv_file(
          spirv_cache_path(path, extension), std::ios::binary
      );
      spv_file.write(
          reinterpret_cast<const char *>(it->second.data()),
          static_cast<std::streamsize>(it->second.size() * sizeof(uint32_t))
      );
    }

    std::string layout_error;
    auto layout_json = serialize_shader_pipeline_layout(
        result.merged_layout, SerializationFormat::Json, &layout_error
    );
    if (layout_json.has_value()) {
      std::ofstream layout_out(layout_path, std::ios::trunc);
      layout_out << *layout_json;
    }

    write_shader_dependencies(path, result.dependencies);
    write_shader_cache_version(path, k_vulkan_shader_cache_format_version);
  }

  return result;
}

bool is_depth_format(FramebufferTextureFormat format) {
  return format == FramebufferTextureFormat::DEPTH24STENCIL8 ||
         format == FramebufferTextureFormat::DEPTH_ONLY;
}

bool is_depth_format(ImageFormat format) {
  return format == ImageFormat::Depth24Stencil8 ||
         format == ImageFormat::Depth32F;
}

VkFormat to_vulkan_format(FramebufferTextureFormat format) {
  switch (format) {
    case FramebufferTextureFormat::RGBA8:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case FramebufferTextureFormat::RGBA16F:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case FramebufferTextureFormat::RGBA32F:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case FramebufferTextureFormat::RGB8:
      return VK_FORMAT_R8G8B8_UNORM;
    case FramebufferTextureFormat::RGB16F:
      return VK_FORMAT_R16G16B16_SFLOAT;
    case FramebufferTextureFormat::RGB32F:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case FramebufferTextureFormat::R32F:
      return VK_FORMAT_R32_SFLOAT;
    case FramebufferTextureFormat::RED_INTEGER:
      return VK_FORMAT_R32_SINT;
    case FramebufferTextureFormat::DEPTH_ONLY:
      return VK_FORMAT_D32_SFLOAT;
    case FramebufferTextureFormat::DEPTH24STENCIL8:
      return VK_FORMAT_D24_UNORM_S8_UINT;
    case FramebufferTextureFormat::None:
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

VkFormat to_vulkan_format(ImageFormat format) {
  switch (format) {
    case ImageFormat::RGBA8:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::RGBA16F:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case ImageFormat::RGBA32F:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case ImageFormat::R32I:
      return VK_FORMAT_R32_SINT;
    case ImageFormat::Depth24Stencil8:
      return VK_FORMAT_D24_UNORM_S8_UINT;
    case ImageFormat::Depth32F:
      return VK_FORMAT_D32_SFLOAT;
    case ImageFormat::Undefined:
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

VkImageAspectFlags to_vulkan_aspect(FramebufferTextureFormat format) {
  switch (format) {
    case FramebufferTextureFormat::DEPTH24STENCIL8:
      return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    case FramebufferTextureFormat::DEPTH_ONLY:
      return VK_IMAGE_ASPECT_DEPTH_BIT;
    default:
      return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

VkImageAspectFlags to_vulkan_aspect(ImageFormat format) {
  switch (format) {
    case ImageFormat::Depth24Stencil8:
      return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    case ImageFormat::Depth32F:
      return VK_IMAGE_ASPECT_DEPTH_BIT;
    case ImageFormat::RGBA8:
    case ImageFormat::RGBA16F:
    case ImageFormat::RGBA32F:
    case ImageFormat::R32I:
    case ImageFormat::Undefined:
    default:
      return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

VkImageUsageFlags to_vulkan_usage(ImageUsage usage, ImageFormat format) {
  VkImageUsageFlags flags = 0;
  const auto usage_bits = static_cast<uint32_t>(usage);
  if (usage_bits & static_cast<uint32_t>(ImageUsage::Sampled)) {
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (usage_bits & static_cast<uint32_t>(ImageUsage::ColorAttachment)) {
    flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
  if (usage_bits & static_cast<uint32_t>(ImageUsage::DepthStencilAttachment)) {
    flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }
  if (usage_bits & static_cast<uint32_t>(ImageUsage::TransferSrc)) {
    flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  if (usage_bits & static_cast<uint32_t>(ImageUsage::TransferDst)) {
    flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }
  if (usage_bits & static_cast<uint32_t>(ImageUsage::Readback)) {
    flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  if (flags == 0) {
    flags = is_depth_format(format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                    : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
  return flags;
}

VkFormat to_vulkan_texture_format(TextureFormat format) {
  switch (format) {
    case TextureFormat::Red:
      return VK_FORMAT_R8_UNORM;
    case TextureFormat::RGB:
    case TextureFormat::RGBA:
    default:
      return VK_FORMAT_R8G8B8A8_UNORM;
  }
}

VkDeviceSize texture_bytes_per_pixel(TextureFormat format) {
  switch (format) {
    case TextureFormat::Red:
      return 1;
    case TextureFormat::RGB:
    case TextureFormat::RGBA:
    default:
      return 4;
  }
}

VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment) {
  if (alignment == 0) {
    return value;
  }

  return (value + alignment - 1) & ~(alignment - 1);
}

bool format_has_stencil(VkFormat format) {
  switch (format) {
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return true;
    default:
      return false;
  }
}

bool is_depth_or_stencil_format(VkFormat format) {
  switch (format) {
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return true;
    default:
      return false;
  }
}

VkImageAspectFlags select_view_aspect(
    ImageAspect aspect,
    VkImageAspectFlags available
) {
  switch (aspect) {
    case ImageAspect::Depth:
      return (available & VK_IMAGE_ASPECT_DEPTH_BIT) != 0
                 ? VK_IMAGE_ASPECT_DEPTH_BIT
                 : available;
    case ImageAspect::Stencil:
      return (available & VK_IMAGE_ASPECT_STENCIL_BIT) != 0
                 ? VK_IMAGE_ASPECT_STENCIL_BIT
                 : available;
    case ImageAspect::Color0:
    case ImageAspect::Color1:
    case ImageAspect::Color2:
    case ImageAspect::Color3:
    default:
      return (available & VK_IMAGE_ASPECT_COLOR_BIT) != 0
                 ? VK_IMAGE_ASPECT_COLOR_BIT
                 : available;
  }
}

VkAttachmentLoadOp to_vulkan_load_op(AttachmentLoadOp op) {
  switch (op) {
    case AttachmentLoadOp::Clear:
      return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case AttachmentLoadOp::DontCare:
      return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case AttachmentLoadOp::Load:
    default:
      return VK_ATTACHMENT_LOAD_OP_LOAD;
  }
}

VkAttachmentStoreOp to_vulkan_store_op(AttachmentStoreOp op) {
  switch (op) {
    case AttachmentStoreOp::DontCare:
      return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case AttachmentStoreOp::Store:
    default:
      return VK_ATTACHMENT_STORE_OP_STORE;
  }
}

VkImageLayout sampled_layout_for_aspect(VkImageAspectFlags aspect) {
  return (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
             ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
             : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

VkImageLayout attachment_layout_for_aspect(VkImageAspectFlags aspect) {
  return (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
             ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
             : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

bool requires_nearest_sampling(VkFormat format) {
  switch (format) {
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_UINT:
      return true;
    default:
      return false;
  }
}

VkSampleCountFlagBits to_vulkan_sample_count(uint32_t samples) {
  switch (samples) {
    case 2:
      return VK_SAMPLE_COUNT_2_BIT;
    case 4:
      return VK_SAMPLE_COUNT_4_BIT;
    case 8:
      return VK_SAMPLE_COUNT_8_BIT;
    case 16:
      return VK_SAMPLE_COUNT_16_BIT;
    case 1:
    default:
      return VK_SAMPLE_COUNT_1_BIT;
  }
}

void hash_append_bytes(uint64_t &seed, const void *data, size_t size) {
  constexpr uint64_t k_offset_basis = 1469598103934665603ull;
  constexpr uint64_t k_prime = 1099511628211ull;

  if (seed == 0) {
    seed = k_offset_basis;
  }

  const auto *bytes = static_cast<const uint8_t *>(data);
  for (size_t i = 0; i < size; ++i) {
    seed ^= static_cast<uint64_t>(bytes[i]);
    seed *= k_prime;
  }
}

template <typename T>
void hash_append_value(uint64_t &seed, const T &value) {
  hash_append_bytes(seed, &value, sizeof(T));
}

} // namespace

size_t VulkanExecutor::DescriptorSetCacheKeyHash::operator()(
    const DescriptorSetCacheKey &key
) const noexcept {
  uint64_t seed = 0;
  hash_append_bytes(
      seed,
      key.layout_key.shader_descriptor_id.data(),
      key.layout_key.shader_descriptor_id.size()
  );
  hash_append_value(seed, key.layout_key.descriptor_set_index);
  hash_append_value(seed, static_cast<uint8_t>(key.reuse_identity.sharing));
  hash_append_bytes(
      seed,
      key.reuse_identity.cache_namespace.data(),
      key.reuse_identity.cache_namespace.size()
  );
  hash_append_value(seed, key.reuse_identity.stable_tag);
  hash_append_value(seed, key.content_hash);
  return static_cast<size_t>(seed);
}

VulkanExecutor::VulkanExecutor(GLFWwindow *window, uint32_t width, uint32_t height)
    : m_width(width), m_height(height) {
  m_instance = std::make_unique<VulkanInstance>();

  if (m_instance->validation_enabled()) {
    m_debug_messenger = std::make_unique<VulkanDebugMessenger>(m_instance->handle());
  }

  m_surface = std::make_unique<VulkanSurface>(m_instance->handle(), window);
  m_device = std::make_unique<VulkanDevice>(m_instance->handle(), m_surface->handle());
  m_swapchain = std::make_unique<VulkanSwapchain>(*m_device, m_surface->handle(), width, height);
  m_frame_context = std::make_unique<VulkanFrameContext>(*m_device, m_swapchain->image_count());
  m_descriptor_allocator = std::make_unique<VulkanDescriptorAllocator>(*m_device);
  m_pipeline_cache = std::make_unique<VulkanPipelineCache>(*m_device);
  for (auto &upload_arena : m_upload_arenas) {
    upload_arena =
        std::make_unique<VulkanUploadArena>(*m_device, UPLOAD_ARENA_SIZE);
  }
  m_swapchain_image_layouts.resize(
      m_swapchain->image_count(), VK_IMAGE_LAYOUT_UNDEFINED
  );
  ASTRA_VK_TRACE_CONTEXT_CREATE(
      m_tracy_graphics_ctx,
      m_device->physical_device(),
      m_device->logical_device(),
      m_device->graphics_queue(),
      m_frame_context->command_buffer()
  );
  ASTRA_VK_TRACE_CONTEXT_NAME(
      m_tracy_graphics_ctx,
      "Vulkan Graphics Queue",
      sizeof("Vulkan Graphics Queue") - 1
  );

  LOG_INFO("[Vulkan] Executor initialized");
}

VulkanExecutor::~VulkanExecutor() {
  if (m_device) {
    vkDeviceWaitIdle(m_device->logical_device());
    ASTRA_VK_TRACE_CONTEXT_DESTROY(m_tracy_graphics_ctx);
    if (m_default_sampler != VK_NULL_HANDLE) {
      vkDestroySampler(m_device->logical_device(), m_default_sampler, nullptr);
      m_default_sampler = VK_NULL_HANDLE;
    }
    if (m_nearest_sampler != VK_NULL_HANDLE) {
      vkDestroySampler(m_device->logical_device(), m_nearest_sampler, nullptr);
      m_nearest_sampler = VK_NULL_HANDLE;
    }
    if (m_depth_sampler != VK_NULL_HANDLE) {
      vkDestroySampler(m_device->logical_device(), m_depth_sampler, nullptr);
      m_depth_sampler = VK_NULL_HANDLE;
    }
  }
  m_shader_registry.clear();
}

void VulkanExecutor::register_shader_program(
    const std::string &descriptor_id,
    Scope<VulkanShaderProgram> program
) {
  m_shader_registry[descriptor_id] = std::move(program);
}

VulkanShaderProgram *VulkanExecutor::find_shader_program(
    const std::string &descriptor_id
) const {
  auto it = m_shader_registry.find(descriptor_id);
  if (it != m_shader_registry.end())
    return it->second.get();
  return nullptr;
}

VulkanShaderProgram *
VulkanExecutor::ensure_shader_program(const CompiledPipeline &pipeline) {
  if (pipeline.vulkan_program != nullptr) {
    return static_cast<VulkanShaderProgram *>(pipeline.vulkan_program);
  }

  if (pipeline.shader_descriptor_id.empty()) {
    return nullptr;
  }

  if (auto *existing = find_shader_program(pipeline.shader_descriptor_id)) {
    return existing;
  }

  auto descriptor = resource_manager()->get_descriptor_by_id<ShaderDescriptor>(
      pipeline.shader_descriptor_id
  );
  ASTRA_ENSURE(
      descriptor == nullptr,
      "[Vulkan] Missing shader descriptor for pipeline '",
      pipeline.debug_name,
      "': ",
      pipeline.shader_descriptor_id
  );

  std::unordered_map<std::string, CompileResult> compiled_sources;
  std::map<StageKind, std::vector<uint32_t>> spirv_stages;
  ShaderPipelineLayout merged_layout;
  LayoutMergeState merge_state;
  bool any_stage_failed = false;
  std::set<std::string> descriptor_source_paths;

  const auto record_descriptor_path = [&](const Ref<Path> &source_path) {
    if (source_path == nullptr) {
      return;
    }

    descriptor_source_paths.insert(path_manager()->resolve(source_path).string());
  };

  record_descriptor_path(descriptor->vertex_path);
  record_descriptor_path(descriptor->fragment_path);
  record_descriptor_path(descriptor->geometry_path);

  std::optional<Compiler> shared_layout_compiler;
  if (descriptor_source_paths.size() > 1) {
    shared_layout_compiler.emplace();
  }

  const auto compile_descriptor_path = [&](const Ref<Path> &source_path) {
    if (source_path == nullptr || any_stage_failed) {
      return;
    }

    const auto resolved_path = path_manager()->resolve(source_path);
    const auto key = resolved_path.string();

    auto [it, inserted] =
        compiled_sources.try_emplace(key, CompileResult{});
    if (inserted) {
      it->second = compile_shader_file(
          resolved_path,
          shared_layout_compiler ? &*shared_layout_compiler : nullptr
      );
      if (!it->second.ok()) {
        std::string error_message;
        for (const auto &error : it->second.errors) {
          if (!error_message.empty()) {
            error_message += "\n";
          }
          error_message += error;
        }
        LOG_WARN("[Vulkan] Skipping shader '{}': {}", pipeline.shader_descriptor_id, error_message);
        any_stage_failed = true;
        return;
      }
    }

    for (const auto &[stage, spirv] : it->second.spirv_stages) {
      spirv_stages[stage] = spirv;
    }
    std::vector<std::string> merge_errors;
    if (!merge_pipeline_layout_checked(
            merged_layout,
            it->second.merged_layout,
            resolved_path.string(),
            merge_state,
            merge_errors
        )) {
      std::string error_message;
      for (const auto &error : merge_errors) {
        if (!error_message.empty()) {
          error_message += "\n";
        }
        error_message += error;
      }
      LOG_WARN(
          "[Vulkan] Skipping shader '{}': {}",
          pipeline.shader_descriptor_id,
          error_message
      );
      any_stage_failed = true;
      return;
    }
  };

  compile_descriptor_path(descriptor->vertex_path);
  compile_descriptor_path(descriptor->fragment_path);
  compile_descriptor_path(descriptor->geometry_path);

  if (any_stage_failed || spirv_stages.empty()) {
    if (spirv_stages.empty()) {
      LOG_WARN("[Vulkan] Shader program produced no SPIR-V stages for pipeline '{}'", pipeline.debug_name);
    }
    return nullptr;
  }

  auto program = std::make_unique<VulkanShaderProgram>(
      *m_device, std::move(spirv_stages), std::move(merged_layout)
  );
  auto *program_ptr = program.get();
  register_shader_program(pipeline.shader_descriptor_id, std::move(program));
  return program_ptr;
}

void VulkanExecutor::recreate_swapchain() {
  m_swapchain->recreate(m_width, m_height);
  m_frame_context->recreate_semaphores(m_swapchain->image_count());
  m_swapchain_image_layouts.assign(
      m_swapchain->image_count(), VK_IMAGE_LAYOUT_UNDEFINED
  );
  m_needs_recreate = false;
}

void VulkanExecutor::execute(const CompiledFrame &frame) {
  ASTRA_PROFILE_N("VulkanExecutor::execute");

  if (m_width == 0 || m_height == 0) {
    return;
  }

  if (m_needs_recreate) {
    ASTRA_PROFILE_N("VulkanExecutor::recreate_swapchain");
    recreate_swapchain();
  }

  VulkanFrameContext::AcquireResult acquire_result =
      VulkanFrameContext::AcquireResult::Success;
  {
    ASTRA_PROFILE_N("VulkanExecutor::acquire_frame");
    acquire_result = m_frame_context->begin_frame(*m_swapchain);
    if (acquire_result == VulkanFrameContext::AcquireResult::NeedsRecreate) {
      recreate_swapchain();
      acquire_result = m_frame_context->begin_frame(*m_swapchain);
      if (acquire_result != VulkanFrameContext::AcquireResult::Success) {
        return;
      }
    }
  }

  {
    ASTRA_PROFILE_N("VulkanExecutor::prepare_frame");
    const uint32_t frame_index = m_frame_context->current_frame_index();
    collect_completed_readbacks(frame_index);
    current_upload_arena().reset();
    m_frame_upload_buffers[frame_index].clear();
    m_uploaded_vertex_buffers.clear();
    m_uploaded_index_buffers.clear();
    m_descriptor_allocator->reset_frame(frame_index);
    m_descriptor_set_cache[frame_index].clear();
  }

  m_frame = &frame;
  m_bound_pipeline = VK_NULL_HANDLE;
  m_bound_pipeline_layout = VK_NULL_HANDLE;
  m_bound_program = nullptr;
  m_active_color_formats.clear();
  m_active_depth_format = VK_FORMAT_UNDEFINED;
  m_active_render_extent = {};

  VkCommandBuffer command_buffer = m_frame_context->command_buffer();

  {
    ASTRA_PROFILE_N("VulkanExecutor::prepare_defaults");
    ensure_default_samplers();
    ensure_default_images_initialized(command_buffer);
  }

  if (frame.passes.empty()) {
    ASTRA_PROFILE_N("VulkanExecutor::clear_empty_frame");
    transition_swapchain_image(
        command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );
    VkRenderingAttachmentInfo clear_attachment{};
    clear_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    clear_attachment.imageView =
        m_swapchain->image_view(m_frame_context->swapchain_image_index());
    clear_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    clear_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    clear_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    clear_attachment.clearValue.color.float32[0] = 0.1f;
    clear_attachment.clearValue.color.float32[1] = 0.1f;
    clear_attachment.clearValue.color.float32[2] = 0.1f;
    clear_attachment.clearValue.color.float32[3] = 1.0f;

    VkRenderingInfo clear_rendering{};
    clear_rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    clear_rendering.renderArea.offset = {0, 0};
    clear_rendering.renderArea.extent = m_swapchain->extent();
    clear_rendering.layerCount = 1;
    clear_rendering.colorAttachmentCount = 1;
    clear_rendering.pColorAttachments = &clear_attachment;

    vkCmdBeginRendering(command_buffer, &clear_rendering);
    vkCmdEndRendering(command_buffer);
  } else {

    for (const auto &pass : frame.passes) {
#ifdef ASTRA_TRACE
      const std::string trace_name = pass_execute_trace_name(pass);
      ASTRA_PROFILE_DYN(trace_name.c_str(), trace_name.size());
#endif
      ASTRA_VK_TRACE_SCOPE_DYN(
          m_tracy_graphics_ctx,
          tracy_gpu_pass_zone,
          command_buffer,
          pass.debug_name.empty() ? "RenderPass" : pass.debug_name.c_str()
      );

      {
        ASTRA_PROFILE_N("VulkanExecutor::prepare_bindings");
        for (const auto &command : pass.commands) {
          if (const auto *bind = std::get_if<BindBindingsCmd>(&command)) {
            const CompiledBindingGroup *binding_group =
                m_frame->find_binding_group(bind->binding_group);
            if (binding_group) {
              for (const auto &image_binding : binding_group->sampled_images) {
                const std::optional<VkImageViewType> preferred_view_type =
                    image_binding.target == CompiledSampledImageTarget::TextureCube
                        ? std::optional<VkImageViewType>{VK_IMAGE_VIEW_TYPE_CUBE}
                        : std::nullopt;
                auto image = resolve_image_view(
                    image_binding.view, false, preferred_view_type
                );
                if (image.valid()) {
                  transition_image(
                      command_buffer, image, sampled_layout_for_aspect(image.aspect)
                  );
                }
              }
            }
          }
        }
      }

      {
        ASTRA_PROFILE_N("VulkanExecutor::record_commands");
        for (const auto &command : pass.commands) {
          std::visit([&](const auto &typed_command) {
            dispatch(command_buffer, typed_command);
          },
                     command);
        }
      }
    }
  }

  {
    ASTRA_PROFILE_N("VulkanExecutor::blit_present_edges");
    ASTRA_VK_TRACE_SCOPE_DYN(
        m_tracy_graphics_ctx,
        tracy_gpu_present_blit_zone,
        command_buffer,
        "PresentBlit"
    );
    blit_present_edges(command_buffer, frame);
  }
  ASTRA_VK_TRACE_COLLECT(m_tracy_graphics_ctx, command_buffer);
  transition_swapchain_image(command_buffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  m_frame_context->end_frame();
  submit_and_present();
  m_frame_context->advance_frame();

  m_frame = nullptr;
}

void VulkanExecutor::handle_resize(uint32_t width, uint32_t height) {
  m_width = width;
  m_height = height;
  m_needs_recreate = true;
}

void VulkanExecutor::submit_and_present() {
  ASTRA_PROFILE_N("VulkanExecutor::submit_and_present");
  VkCommandBuffer command_buffer = m_frame_context->command_buffer();
  VkSemaphore wait_semaphore = m_frame_context->image_available_semaphore();
  VkSemaphore signal_semaphore = m_frame_context->render_finished_semaphore();
  VkFence fence = m_frame_context->submit_fence();

  VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_TRANSFER_BIT;

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &wait_semaphore;
  submit_info.pWaitDstStageMask = &wait_stage;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &signal_semaphore;

  VkResult result = vkQueueSubmit(m_device->graphics_queue(), 1, &submit_info, fence);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to submit command buffer, VkResult: ", static_cast<int>(result));

  VkSwapchainKHR swapchain = m_swapchain->handle();
  uint32_t image_index = m_frame_context->swapchain_image_index();

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &signal_semaphore;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain;
  present_info.pImageIndices = &image_index;

  result = vkQueuePresentKHR(m_device->present_queue(), &present_info);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    m_needs_recreate = true;
    return;
  }

  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to present, VkResult: ", static_cast<int>(result));
}

void VulkanExecutor::ensure_default_samplers() {
  if (m_default_sampler != VK_NULL_HANDLE &&
      m_nearest_sampler != VK_NULL_HANDLE &&
      m_depth_sampler != VK_NULL_HANDLE) {
    return;
  }

  const auto create_sampler = [&](VkFilter filter,
                                  VkSamplerMipmapMode mipmap_mode,
                                  VkSamplerAddressMode address_mode,
                                  const float *border_color,
                                  VkSampler *out_sampler) {
    VkSamplerCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    create_info.magFilter = filter;
    create_info.minFilter = filter;
    create_info.mipmapMode = mipmap_mode;
    create_info.addressModeU = address_mode;
    create_info.addressModeV = address_mode;
    create_info.addressModeW = address_mode;
    create_info.maxLod = 1.0f;
    if (border_color != nullptr) {
      create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    }

    VkResult result = vkCreateSampler(
        m_device->logical_device(), &create_info, nullptr, out_sampler
    );
    ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create sampler");
  };

  if (m_default_sampler == VK_NULL_HANDLE) {
    create_sampler(
        VK_FILTER_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        nullptr,
        &m_default_sampler
    );
  }
  if (m_nearest_sampler == VK_NULL_HANDLE) {
    create_sampler(
        VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        nullptr,
        &m_nearest_sampler
    );
  }
  if (m_depth_sampler == VK_NULL_HANDLE) {
    static const float k_border_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    create_sampler(
        VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        k_border_color,
        &m_depth_sampler
    );
  }

  if (!m_default_image) {
    m_default_image = std::make_unique<VulkanImage>(
        *m_device,
        VulkanImage::CreateInfo{
            .width = 1,
            .height = 1,
            .array_layers = 1,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .view_type = VK_IMAGE_VIEW_TYPE_2D,
        }
    );
  }
}

void VulkanExecutor::ensure_default_images_initialized(
    VkCommandBuffer command_buffer
) {
  ensure_default_samplers();

  const auto ensure_color_image = [&](
                                      Scope<VulkanImage> &image,
                                      const std::array<float, 4> &clear_color
                                  ) {
    if (!image) {
      image = std::make_unique<VulkanImage>(
          *m_device,
          VulkanImage::CreateInfo{
              .width = 1,
              .height = 1,
              .array_layers = 1,
              .format = VK_FORMAT_R8G8B8A8_UNORM,
              .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
              .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .view_type = VK_IMAGE_VIEW_TYPE_2D,
          }
      );
    }

    if (image->current_layout() != VK_IMAGE_LAYOUT_UNDEFINED) {
      return;
    }

    image->transition(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkClearColorValue color{};
    color.float32[0] = clear_color[0];
    color.float32[1] = clear_color[1];
    color.float32[2] = clear_color[2];
    color.float32[3] = clear_color[3];

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    vkCmdClearColorImage(
        command_buffer,
        image->handle(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &color,
        1,
        &range
    );
    image->transition(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  };

  ensure_color_image(m_default_image, {1.0f, 1.0f, 1.0f, 1.0f});
  ensure_color_image(m_black_image, {0.0f, 0.0f, 0.0f, 1.0f});
  ensure_color_image(m_flat_normal_image, {0.5f, 0.5f, 1.0f, 1.0f});
}

VulkanExecutor::ResolvedImageResource
VulkanExecutor::resolve_default_sampler_image(
    const ShaderResourceBindingDesc &resource
) const {
  const VulkanImage *image = m_default_image.get();
  if (resource_uses_flat_normal_default(resource.logical_name)) {
    image = m_flat_normal_image.get();
  } else if (resource_uses_black_default(resource.logical_name)) {
    image = m_black_image.get();
  }

  if (image == nullptr) {
    return {};
  }

  return ResolvedImageResource{
      .image = image->handle(),
      .view = image->view(),
      .format = image->format(),
      .extent = VkExtent2D{image->width(), image->height()},
      .aspect = static_cast<VkImageAspectFlags>(image->aspect()),
      .base_mip_level = 0,
      .level_count = image->mip_levels(),
      .base_array_layer = 0,
      .layer_count = image->array_layers(),
      .owned_image = const_cast<VulkanImage *>(image),
  };
}

VkSampler VulkanExecutor::sampler_for_format(
    VkFormat format,
    VkImageAspectFlags aspect
) {
  ensure_default_samplers();
  if ((aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0 ||
      is_depth_or_stencil_format(format)) {
    return m_depth_sampler;
  }
  return requires_nearest_sampling(format) ? m_nearest_sampler
                                           : m_default_sampler;
}

VulkanUploadArena &VulkanExecutor::current_upload_arena() {
  return *m_upload_arenas[m_frame_context->current_frame_index()];
}

VulkanUploadArena::Allocation VulkanExecutor::allocate_upload_allocation(
    VkDeviceSize size, VkDeviceSize alignment
) {
  auto &upload_arena = current_upload_arena();
  if (upload_arena.can_allocate(size, alignment)) {
    return upload_arena.allocate(size, alignment);
  }

  auto staging_buffer = std::make_unique<VulkanBuffer>(
      *m_device,
      size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );
  staging_buffer->map();

  VulkanUploadArena::Allocation allocation{};
  allocation.buffer = staging_buffer->handle();
  allocation.offset = 0;
  allocation.mapped = staging_buffer->mapped();

  m_frame_upload_buffers[m_frame_context->current_frame_index()].push_back(
      std::move(staging_buffer)
  );
  return allocation;
}

void VulkanExecutor::transition_swapchain_image(
    VkCommandBuffer command_buffer,
    VkImageLayout new_layout
) {
  transition_image(
      command_buffer,
      ResolvedImageResource{
          .image = m_swapchain->image(m_frame_context->swapchain_image_index()),
          .view = m_swapchain->image_view(m_frame_context->swapchain_image_index()),
          .format = m_swapchain->image_format(),
          .extent = m_swapchain->extent(),
          .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
          .owned_image = nullptr,
          .swapchain_image_index = m_frame_context->swapchain_image_index(),
      },
      new_layout
  );
}

VkImageLayout VulkanExecutor::tracked_layout_for(
    const ResolvedImageResource &image
) const {
  if (!image.valid()) {
    return VK_IMAGE_LAYOUT_UNDEFINED;
  }

  return image.is_swapchain()
             ? m_swapchain_image_layouts[image.swapchain_image_index]
             : image.owned_image->current_layout();
}

void VulkanExecutor::transition_image(
    VkCommandBuffer command_buffer,
    const ResolvedImageResource &image,
    VkImageLayout new_layout
) {
  if (!image.valid()) {
    return;
  }

  const VkImageLayout old_layout = tracked_layout_for(image);
  if (old_layout == new_layout) {
    return;
  }

  VkImageAspectFlags barrier_aspect = image.aspect;
  if (image.owned_image != nullptr &&
      (image.owned_image->aspect() &
       (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) ==
          (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
    barrier_aspect = image.owned_image->aspect();
  }

  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image.image;
  barrier.subresourceRange.aspectMask = barrier_aspect;
  barrier.subresourceRange.baseMipLevel = image.base_mip_level;
  barrier.subresourceRange.levelCount = image.level_count;
  barrier.subresourceRange.baseArrayLayer = image.base_array_layer;
  barrier.subresourceRange.layerCount = image.layer_count;
  barrier.srcStageMask = old_layout == VK_IMAGE_LAYOUT_UNDEFINED
                             ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT
                             : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.srcAccessMask = old_layout == VK_IMAGE_LAYOUT_UNDEFINED
                              ? 0
                              : (VK_ACCESS_2_MEMORY_WRITE_BIT |
                                 VK_ACCESS_2_MEMORY_READ_BIT);
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  VkDependencyInfo dependency_info{};
  dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency_info.imageMemoryBarrierCount = 1;
  dependency_info.pImageMemoryBarriers = &barrier;

  vkCmdPipelineBarrier2(command_buffer, &dependency_info);

  if (image.is_swapchain()) {
    m_swapchain_image_layouts[image.swapchain_image_index] = new_layout;
  } else {
    image.owned_image->set_current_layout(new_layout);
  }
}

void VulkanExecutor::clear_bound_pipeline_state() {
  m_current_pipeline = nullptr;
  m_bound_pipeline = VK_NULL_HANDLE;
  m_bound_pipeline_layout = VK_NULL_HANDLE;
  m_bound_program = nullptr;
  m_bound_vertex_layout = {};
  m_has_bound_vertex_layout = false;
}

bool VulkanExecutor::try_bind_current_pipeline(VkCommandBuffer command_buffer) {
  if (m_current_pipeline == nullptr || m_bound_program == nullptr) {
    return false;
  }

  if (m_active_color_formats.empty() &&
      m_active_depth_format == VK_FORMAT_UNDEFINED) {
    return false;
  }

  const bool needs_vertex_layout =
      !m_bound_program->vertex_input().attributes.empty();
  const BufferLayout *vertex_layout =
      m_has_bound_vertex_layout ? &m_bound_vertex_layout : nullptr;
  if (needs_vertex_layout && vertex_layout == nullptr) {
    m_bound_pipeline = VK_NULL_HANDLE;
    return false;
  }

  VkPipeline pipeline = m_pipeline_cache->get_or_create_graphics_pipeline(
      *m_bound_program, m_current_pipeline->desc, vertex_layout, m_active_color_formats, m_active_depth_format
  );
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  m_bound_pipeline = pipeline;
  m_bound_pipeline_layout = m_bound_program->pipeline_layout();
  return true;
}

bool VulkanExecutor::should_cache_binding_group(
    const CompiledBindingGroup &binding_group
) const {
  switch (binding_group.cache_policy) {
    case RenderBindingCachePolicy::Reuse:
      return true;
    case RenderBindingCachePolicy::Ephemeral:
      return false;
    case RenderBindingCachePolicy::Auto:
    default:
      return binding_group.scope != RenderBindingScope::Draw &&
             binding_group.stability != RenderBindingStability::Transient;
  }
}

void VulkanExecutor::collect_completed_readbacks(uint32_t frame_index) {
  if (frame_index >= m_pending_readbacks.size()) {
    return;
  }

  auto &pending_readbacks = m_pending_readbacks[frame_index];
  if (pending_readbacks.empty()) {
    return;
  }

  for (auto &pending : pending_readbacks) {
    if (pending.out_value == nullptr || pending.buffer == nullptr) {
      continue;
    }

    pending.buffer->map();
    const auto *bytes =
        static_cast<const uint8_t *>(pending.buffer->mapped());
    if (bytes == nullptr) {
      continue;
    }

    if (pending.format == VK_FORMAT_R32_SINT) {
      std::memcpy(pending.out_value, bytes, sizeof(int32_t));
    } else if (pending.format == VK_FORMAT_R32_UINT) {
      uint32_t value = 0;
      std::memcpy(&value, bytes, sizeof(uint32_t));
      *pending.out_value = static_cast<int>(value);
    } else {
      std::memcpy(pending.out_value, bytes, sizeof(int32_t));
    }

    if (pending.out_ready != nullptr) {
      *pending.out_ready = true;
    }

    pending.buffer->unmap();
  }

  pending_readbacks.clear();
}

bool VulkanExecutor::image_requires_depth_layout(
    const ResolvedImageResource &image
) const {
  return (image.aspect & (VK_IMAGE_ASPECT_DEPTH_BIT |
                          VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
}

VulkanExecutor::ResolvedImageResource
VulkanExecutor::resolve_texture_2d(const CompiledImage &compiled_image, const Texture &texture) {
  const auto *virtual_texture =
      dynamic_cast<const VirtualTexture2D *>(&texture);
  if (virtual_texture == nullptr) {
    LOG_WARN("[Vulkan] Texture '{}' is not a Vulkan-compatible 2D texture", compiled_image.debug_name);
    return {};
  }

  const uint32_t width = std::max(virtual_texture->width(), 1u);
  const uint32_t height = std::max(virtual_texture->height(), 1u);
  const VkFormat format = to_vulkan_texture_format(virtual_texture->format());
  auto &image = m_texture_images[&texture];

  const bool needs_upload =
      !image || image->width() != width || image->height() != height ||
      image->format() != format || image->array_layers() != 1;
  if (needs_upload) {
    image = std::make_unique<VulkanImage>(
        *m_device,
        VulkanImage::CreateInfo{
            .width = width,
            .height = height,
            .array_layers = 1,
            .format = format,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .view_type = VK_IMAGE_VIEW_TYPE_2D,
        }
    );

    ResolvedImageResource resolved{
        .image = image->handle(),
        .view = image->view(),
        .format = image->format(),
        .extent = VkExtent2D{width, height},
        .aspect = static_cast<VkImageAspectFlags>(image->aspect()),
        .base_mip_level = 0,
        .level_count = image->mip_levels(),
        .base_array_layer = 0,
        .layer_count = image->array_layers(),
        .owned_image = image.get(),
    };

    const uint32_t pixel_count = width * height;
    const VkDeviceSize upload_size = 4 * static_cast<VkDeviceSize>(pixel_count);
    auto allocation = allocate_upload_allocation(upload_size, 4);
    std::memset(allocation.mapped, 255, static_cast<size_t>(upload_size));

    const auto &bytes = virtual_texture->bytes();
    if (!bytes.empty()) {
      if (virtual_texture->format() == TextureFormat::RGB) {
        auto *destination = static_cast<uint8_t *>(allocation.mapped);
        const auto *source = bytes.data();
        const size_t source_pixel_count = std::min<size_t>(bytes.size() / 3, pixel_count);
        for (size_t i = 0; i < source_pixel_count; ++i) {
          destination[i * 4 + 0] = source[i * 3 + 0];
          destination[i * 4 + 1] = source[i * 3 + 1];
          destination[i * 4 + 2] = source[i * 3 + 2];
          destination[i * 4 + 3] = 255;
        }
      } else {
        const auto copy_size = std::min<VkDeviceSize>(
            upload_size, static_cast<VkDeviceSize>(bytes.size())
        );
        std::memcpy(allocation.mapped, bytes.data(), static_cast<size_t>(copy_size));
      }
    }

    VkCommandBuffer command_buffer = m_frame_context->command_buffer();
    transition_image(command_buffer, resolved, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy region{};
    region.bufferOffset = allocation.offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(command_buffer, allocation.buffer, image->handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    transition_image(command_buffer, resolved, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  return ResolvedImageResource{
      .image = image->handle(),
      .view = image->view(),
      .format = image->format(),
      .extent = VkExtent2D{width, height},
      .aspect = static_cast<VkImageAspectFlags>(image->aspect()),
      .base_mip_level = 0,
      .level_count = image->mip_levels(),
      .base_array_layer = 0,
      .layer_count = image->array_layers(),
      .owned_image = image.get(),
  };
}

VulkanExecutor::ResolvedImageResource
VulkanExecutor::resolve_texture_cube(const CompiledImage &compiled_image, const Texture &texture) {
  const auto *virtual_texture =
      dynamic_cast<const VirtualTexture3D *>(&texture);
  if (virtual_texture == nullptr) {
    LOG_WARN("[Vulkan] Texture '{}' is not a Vulkan-compatible cubemap", compiled_image.debug_name);
    return {};
  }

  const uint32_t layer_count = std::max(virtual_texture->face_count(), 1u);
  const uint32_t width = std::max(virtual_texture->width(), 1u);
  const uint32_t height = std::max(virtual_texture->height(), 1u);
  const VkFormat format = to_vulkan_texture_format(virtual_texture->format());
  auto &image = m_texture_images[&texture];

  const bool needs_upload =
      !image || image->width() != width || image->height() != height ||
      image->format() != format || image->array_layers() != layer_count;
  if (needs_upload) {
    image = std::make_unique<VulkanImage>(
        *m_device,
        VulkanImage::CreateInfo{
            .width = width,
            .height = height,
            .array_layers = layer_count,
            .format = format,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .view_type = VK_IMAGE_VIEW_TYPE_CUBE,
        }
    );

    ResolvedImageResource resolved{
        .image = image->handle(),
        .view = image->view(),
        .format = image->format(),
        .extent = VkExtent2D{width, height},
        .aspect = static_cast<VkImageAspectFlags>(image->aspect()),
        .base_mip_level = 0,
        .level_count = image->mip_levels(),
        .base_array_layer = 0,
        .layer_count = image->array_layers(),
        .owned_image = image.get(),
    };

    const uint32_t face_pixel_count = width * height;
    const VkDeviceSize face_size = 4 * static_cast<VkDeviceSize>(face_pixel_count);
    const VkDeviceSize aligned_face_size = align_up(face_size, 4);
    const VkDeviceSize upload_size = aligned_face_size * layer_count;
    auto allocation = allocate_upload_allocation(upload_size, 4);
    std::memset(allocation.mapped, 255, static_cast<size_t>(upload_size));

    const bool source_is_rgb = virtual_texture->format() == TextureFormat::RGB;
    const auto &faces = virtual_texture->faces();
    for (uint32_t layer = 0; layer < layer_count; ++layer) {
      if (layer >= faces.size())
        continue;

      auto *destination = static_cast<uint8_t *>(allocation.mapped) +
                          static_cast<size_t>(aligned_face_size * layer);
      if (source_is_rgb) {
        const auto *source = faces[layer].data();
        const size_t source_pixel_count = std::min<size_t>(faces[layer].size() / 3, face_pixel_count);
        for (size_t i = 0; i < source_pixel_count; ++i) {
          destination[i * 4 + 0] = source[i * 3 + 0];
          destination[i * 4 + 1] = source[i * 3 + 1];
          destination[i * 4 + 2] = source[i * 3 + 2];
          destination[i * 4 + 3] = 255;
        }
      } else {
        const auto copy_size = std::min<VkDeviceSize>(
            face_size, static_cast<VkDeviceSize>(faces[layer].size())
        );
        if (copy_size > 0)
          std::memcpy(destination, faces[layer].data(), static_cast<size_t>(copy_size));
      }
    }

    VkCommandBuffer command_buffer = m_frame_context->command_buffer();
    transition_image(command_buffer, resolved, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    std::vector<VkBufferImageCopy> regions(layer_count);
    for (uint32_t layer = 0; layer < layer_count; ++layer) {
      auto &region = regions[layer];
      region.bufferOffset = allocation.offset + aligned_face_size * layer;
      region.bufferRowLength = 0;
      region.bufferImageHeight = 0;
      region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.imageSubresource.mipLevel = 0;
      region.imageSubresource.baseArrayLayer = layer;
      region.imageSubresource.layerCount = 1;
      region.imageOffset = {0, 0, 0};
      region.imageExtent = {width, height, 1};
    }

    vkCmdCopyBufferToImage(command_buffer, allocation.buffer, image->handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layer_count, regions.data());
    transition_image(command_buffer, resolved, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  return ResolvedImageResource{
      .image = image->handle(),
      .view = image->view(),
      .format = image->format(),
      .extent = VkExtent2D{width, height},
      .aspect = static_cast<VkImageAspectFlags>(image->aspect()),
      .base_mip_level = 0,
      .level_count = image->mip_levels(),
      .base_array_layer = 0,
      .layer_count = image->array_layers(),
      .owned_image = image.get(),
  };
}

VulkanExecutor::ResolvedImageResource
VulkanExecutor::resolve_graph_image(const CompiledImage &compiled_image) {
  if (compiled_image.graph_image == nullptr) {
    return {};
  }

  const auto *key = compiled_image.graph_image.get();
  const auto &desc = compiled_image.graph_image->desc;
  const VkFormat format = to_vulkan_format(desc.format);
  if (format == VK_FORMAT_UNDEFINED) {
    LOG_WARN("[Vulkan] Graph image '{}' has no Vulkan format", compiled_image.debug_name);
    return {};
  }

  auto &image = m_graph_images[key];
  const bool needs_recreate =
      !image || image->width() != desc.width || image->height() != desc.height ||
      image->format() != format ||
      image->samples() != to_vulkan_sample_count(desc.samples) ||
      image->mip_levels() != std::max(desc.mip_levels, 1u) ||
      m_graph_image_generations[key] != compiled_image.graph_image->generation;
  if (needs_recreate) {
    image = std::make_unique<VulkanImage>(
        *m_device,
        VulkanImage::CreateInfo{
            .width = desc.width,
            .height = desc.height,
            .mip_levels = std::max(desc.mip_levels, 1u),
            .format = format,
            .usage = to_vulkan_usage(desc.usage, desc.format),
            .aspect = to_vulkan_aspect(desc.format),
            .samples = to_vulkan_sample_count(desc.samples),
        }
    );
    m_graph_image_generations[key] = compiled_image.graph_image->generation;
  }

  return ResolvedImageResource{
      .image = image->handle(),
      .view = image->view(),
      .format = image->format(),
      .extent = VkExtent2D{desc.width, desc.height},
      .aspect = static_cast<VkImageAspectFlags>(image->aspect()),
      .base_mip_level = 0,
      .level_count = image->mip_levels(),
      .base_array_layer = 0,
      .layer_count = image->array_layers(),
      .owned_image = image.get(),
  };
}

VulkanExecutor::ResolvedImageResource
VulkanExecutor::resolve_image_handle(ImageHandle handle) {
  if (m_frame == nullptr) {
    return {};
  }

  const CompiledImage *compiled_image = m_frame->find_image(handle);
  if (compiled_image == nullptr) {
    LOG_WARN("[Vulkan] Unknown image handle {}", handle.id);
    return {};
  }

  switch (compiled_image->source) {
    case CompiledImageSourceKind::DefaultColorTarget:
      return ResolvedImageResource{
          .image = m_swapchain->image(m_frame_context->swapchain_image_index()),
          .view = m_swapchain->image_view(m_frame_context->swapchain_image_index()),
          .format = m_swapchain->image_format(),
          .extent = m_swapchain->extent(),
          .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
          .base_mip_level = 0,
          .level_count = 1,
          .base_array_layer = 0,
          .layer_count = 1,
          .owned_image = nullptr,
          .swapchain_image_index = m_frame_context->swapchain_image_index(),
      };
    case CompiledImageSourceKind::GraphImage:
      return resolve_graph_image(*compiled_image);
    case CompiledImageSourceKind::Texture2DResource:
      if (compiled_image->texture == nullptr) {
        LOG_WARN("[Vulkan] Texture '{}' is null", compiled_image->debug_name);
        return {};
      }
      return resolve_texture_2d(*compiled_image, *compiled_image->texture);
    case CompiledImageSourceKind::TextureCubeResource:
      if (compiled_image->texture == nullptr) {
        LOG_WARN("[Vulkan] Texture '{}' is null", compiled_image->debug_name);
        return {};
      }
      return resolve_texture_cube(*compiled_image, *compiled_image->texture);
    case CompiledImageSourceKind::RawTextureId:
      LOG_WARN(
          "[Vulkan] Image '{}' is not backed by a Vulkan image yet",
          compiled_image->debug_name
      );
      return {};
    default:
      return {};
  }
}

VulkanExecutor::ResolvedImageResource
VulkanExecutor::resolve_image_view(
    const ImageViewRef &view,
    bool exact_subresource,
    std::optional<VkImageViewType> preferred_view_type
) {
  if (m_frame == nullptr) {
    return {};
  }

  const CompiledImage *compiled_image = m_frame->find_image(view.image);
  if (compiled_image == nullptr) {
    LOG_WARN("[Vulkan] Unknown image handle {}", view.image.id);
    return {};
  }

  const auto finalize_view = [&](ResolvedImageResource resolved)
      -> ResolvedImageResource {
    if (!resolved.valid()) {
      return resolved;
    }

    resolved.aspect = select_view_aspect(view.aspect, resolved.aspect);
    if (resolved.owned_image == nullptr) {
      return resolved;
    }

    const uint32_t mip_levels = resolved.owned_image->mip_levels();
    if (view.mip >= mip_levels) {
      LOG_WARN(
          "[Vulkan] Image '{}' requested mip {} but only has {} levels",
          compiled_image->debug_name,
          view.mip,
          mip_levels
      );
      return {};
    }

    const uint32_t array_layers = resolved.owned_image->array_layers();
    if (view.layer >= array_layers && view.layer != 0) {
      LOG_WARN(
          "[Vulkan] Image '{}' requested layer {} but only has {} layers",
          compiled_image->debug_name,
          view.layer,
          array_layers
      );
      return {};
    }

    resolved.base_mip_level = view.mip;
    resolved.level_count =
        exact_subresource ? 1u : std::max(1u, mip_levels - view.mip);
    resolved.base_array_layer = view.layer;
    resolved.layer_count =
        exact_subresource ? 1u
                          : (view.layer == 0 ? array_layers : 1u);
    resolved.view = resolved.owned_image->view_for_subresource(
        resolved.aspect,
        resolved.base_mip_level,
        resolved.level_count,
        resolved.base_array_layer,
        resolved.layer_count,
        preferred_view_type
    );
    return resolved;
  };

  return finalize_view(resolve_image_handle(view.image));
}

void VulkanExecutor::blit_present_edges(
    VkCommandBuffer command_buffer,
    const CompiledFrame &frame
) {
  if (frame.present_edges.empty()) {
    return;
  }

  for (const auto &edge : frame.present_edges) {
    if (!edge.source.image.valid() || edge.extent.width == 0 ||
        edge.extent.height == 0) {
      continue;
    }

    auto source = resolve_image_view(edge.source, true);
    ResolvedImageResource destination{
        .image = m_swapchain->image(m_frame_context->swapchain_image_index()),
        .view = m_swapchain->image_view(m_frame_context->swapchain_image_index()),
        .format = m_swapchain->image_format(),
        .extent = m_swapchain->extent(),
        .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
        .owned_image = nullptr,
        .swapchain_image_index = m_frame_context->swapchain_image_index(),
    };

    if (!source.valid()) {
      continue;
    }

    transition_image(command_buffer, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition_image(
        command_buffer, destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    VkImageBlit region{};
    region.srcSubresource.aspectMask = source.aspect;
    region.srcSubresource.mipLevel = source.base_mip_level;
    region.srcSubresource.baseArrayLayer = source.base_array_layer;
    region.srcSubresource.layerCount = 1;
    region.srcOffsets[1] = {
        static_cast<int32_t>(edge.extent.width),
        static_cast<int32_t>(edge.extent.height),
        1,
    };
    region.dstSubresource.aspectMask = destination.aspect;
    region.dstSubresource.mipLevel = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 1;
    region.dstOffsets[1] = {
        static_cast<int32_t>(destination.extent.width),
        static_cast<int32_t>(destination.extent.height),
        1,
    };

    vkCmdBlitImage(
        command_buffer,
        source.image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        destination.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region,
        VK_FILTER_NEAREST
    );
  }
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const BeginRenderingCmd &command) {
  clear_bound_pipeline_state();

  std::vector<VkRenderingAttachmentInfo> color_attachments;
  std::vector<ResolvedImageResource> resolved_color_images;
  color_attachments.reserve(command.info.color_attachments.size());
  resolved_color_images.reserve(command.info.color_attachments.size());
  m_active_color_formats.clear();
  m_active_depth_format = VK_FORMAT_UNDEFINED;
  m_active_render_extent = {
      command.info.extent.width,
      command.info.extent.height,
  };

  for (const auto &attachment_ref : command.info.color_attachments) {
    auto image = resolve_image_view(attachment_ref.view, true);
    if (!image.valid()) {
      continue;
    }

    transition_image(
        command_buffer, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    VkRenderingAttachmentInfo attachment{};
    attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachment.imageView = image.view;
    attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.loadOp = to_vulkan_load_op(attachment_ref.load_op);
    attachment.storeOp = to_vulkan_store_op(attachment_ref.store_op);
    attachment.clearValue.color.float32[0] = attachment_ref.clear_color[0];
    attachment.clearValue.color.float32[1] = attachment_ref.clear_color[1];
    attachment.clearValue.color.float32[2] = attachment_ref.clear_color[2];
    attachment.clearValue.color.float32[3] = attachment_ref.clear_color[3];
    resolved_color_images.push_back(image);
    color_attachments.push_back(attachment);
    m_active_color_formats.push_back(image.format);
    if (m_active_render_extent.width == 0 || m_active_render_extent.height == 0) {
      m_active_render_extent = image.extent;
    }
  }

  std::optional<VkRenderingAttachmentInfo> depth_attachment;
  std::optional<VkRenderingAttachmentInfo> stencil_attachment;
  if (command.info.depth_stencil_attachment.has_value()) {
    auto image = resolve_image_view(
        command.info.depth_stencil_attachment->view, true
    );
    if (image.valid()) {
      transition_image(
          command_buffer, image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
      );

      VkRenderingAttachmentInfo attachment{};
      attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
      attachment.imageView = image.view;
      attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      attachment.loadOp = to_vulkan_load_op(
          command.info.depth_stencil_attachment->depth_load_op
      );
      attachment.storeOp = to_vulkan_store_op(
          command.info.depth_stencil_attachment->depth_store_op
      );
      attachment.clearValue.depthStencil.depth =
          command.info.depth_stencil_attachment->clear_depth;
      attachment.clearValue.depthStencil.stencil =
          command.info.depth_stencil_attachment->clear_stencil;

      depth_attachment = attachment;
      if (format_has_stencil(image.format)) {
        auto stencil_image = resolve_image_view(
            ImageViewRef{
                .image = command.info.depth_stencil_attachment->view.image,
                .aspect = ImageAspect::Stencil,
                .mip = command.info.depth_stencil_attachment->view.mip,
                .layer = command.info.depth_stencil_attachment->view.layer,
            },
            true
        );
        if (stencil_image.valid()) {
          VkRenderingAttachmentInfo stencil = attachment;
          stencil.imageView = stencil_image.view;
          stencil.loadOp = to_vulkan_load_op(
              command.info.depth_stencil_attachment->stencil_load_op
          );
          stencil.storeOp = to_vulkan_store_op(
              command.info.depth_stencil_attachment->stencil_store_op
          );
          stencil_attachment = stencil;
        }
      }
      m_active_depth_format = image.format;
      if (m_active_render_extent.width == 0 || m_active_render_extent.height == 0) {
        m_active_render_extent = image.extent;
      }
    }
  }

  ASTRA_ENSURE(
      color_attachments.empty() && !depth_attachment.has_value(),
      "[Vulkan] BeginRenderingCmd requires at least one valid attachment"
  );

  VkRenderingInfo rendering_info{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea.offset = {0, 0};
  rendering_info.renderArea.extent = m_active_render_extent;
  rendering_info.layerCount = 1;
  rendering_info.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size());
  rendering_info.pColorAttachments = color_attachments.data();

  if (depth_attachment.has_value()) {
    rendering_info.pDepthAttachment = &*depth_attachment;
    if (stencil_attachment.has_value()) {
      rendering_info.pStencilAttachment = &*stencil_attachment;
    }
  }

  vkCmdBeginRendering(command_buffer, &rendering_info);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = static_cast<float>(m_active_render_extent.height);
  viewport.width = static_cast<float>(m_active_render_extent.width);
  viewport.height = -static_cast<float>(m_active_render_extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = m_active_render_extent;
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const EndRenderingCmd &command) {
  (void)command;
  vkCmdEndRendering(command_buffer);
  clear_bound_pipeline_state();
  m_active_color_formats.clear();
  m_active_depth_format = VK_FORMAT_UNDEFINED;
  m_active_render_extent = {};
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const BindPipelineCmd &command) {
  const CompiledPipeline *compiled_pipeline = m_frame->find_pipeline(command.pipeline);
  if (!compiled_pipeline) {
    LOG_WARN("[Vulkan] BindPipeline: unknown pipeline handle {}", command.pipeline.id);
    clear_bound_pipeline_state();
    return;
  }

  auto *program = static_cast<VulkanShaderProgram *>(compiled_pipeline->vulkan_program);
  if (!program && !compiled_pipeline->shader_descriptor_id.empty()) {
    program = ensure_shader_program(*compiled_pipeline);
    if (program != nullptr) {
      const_cast<CompiledPipeline *>(compiled_pipeline)->vulkan_program = program;
    }
  }
  if (!program) {
    LOG_WARN("[Vulkan] BindPipeline: pipeline '{}' has no Vulkan shader program", compiled_pipeline->debug_name);
    clear_bound_pipeline_state();
    return;
  }

  if (m_active_color_formats.empty() &&
      m_active_depth_format == VK_FORMAT_UNDEFINED) {
    LOG_WARN("[Vulkan] BindPipeline: no active render attachments");
    clear_bound_pipeline_state();
    return;
  }

  m_current_pipeline = compiled_pipeline;
  m_bound_pipeline_layout = program->pipeline_layout();
  m_bound_program = program;
  m_bound_pipeline = VK_NULL_HANDLE;

  if (program->vertex_input().attributes.empty()) {
    try_bind_current_pipeline(command_buffer);
  }
}

void VulkanExecutor::dispatch(
    VkCommandBuffer command_buffer,
    const BindComputePipelineCmd &command
) {
  (void)command_buffer;
  (void)command;
  ASTRA_EXCEPTION("Vulkan compute pipelines are not implemented yet");
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const BindBindingsCmd &command) {
  if (!m_bound_program) {
    LOG_WARN("[Vulkan] BindBindings: no pipeline bound");
    return;
  }

  const CompiledBindingGroup *binding_group =
      m_frame->find_binding_group(command.binding_group);
  if (!binding_group) {
    LOG_WARN(
        "[Vulkan] BindBindings: unknown binding group handle {}",
        command.binding_group.id
    );
    return;
  }

  if (!binding_group->layout_key.shader_descriptor_id.empty() &&
      m_current_pipeline != nullptr &&
      binding_group->layout_key.shader_descriptor_id !=
          m_current_pipeline->shader_descriptor_id) {
    LOG_WARN(
        "[Vulkan] BindBindings: binding group '{}' targets shader '{}' but "
        "current pipeline uses '{}'",
        binding_group->debug_name,
        binding_group->layout_key.shader_descriptor_id,
        m_current_pipeline->shader_descriptor_id
    );
    return;
  }

  const auto &layout = m_bound_program->layout();
  const uint32_t set = binding_group->layout_key.descriptor_set_index;
  VkDescriptorSetLayout set_layout = m_bound_program->descriptor_set_layout(set);
  if (set_layout == VK_NULL_HANDLE) {
    LOG_WARN(
        "[Vulkan] BindBindings: shader '{}' has no descriptor set layout for set {}",
        m_current_pipeline != nullptr ? m_current_pipeline->debug_name : "<unknown>",
        set
    );
    return;
  }

  std::vector<VkWriteDescriptorSet> writes;
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  std::vector<VkDescriptorImageInfo> image_infos;

  buffer_infos.reserve(
      binding_group->values.size() + layout.resource_layout.resources.size()
  );
  image_infos.reserve(
      binding_group->sampled_images.size() +
      layout.resource_layout.resources.size()
  );

  struct PendingBlockWrite {
    const ShaderValueBlockDesc *block = nullptr;
    std::vector<uint8_t> ubo_data;
  };
  std::map<uint64_t, PendingBlockWrite> pending_blocks;

  auto find_block_for_field = [&](uint64_t binding_id)
      -> std::pair<const ShaderValueBlockDesc *, const ShaderValueFieldDesc *> {
    for (const auto &block : layout.resource_layout.value_blocks) {
      for (const auto &field : block.fields) {
        if (field.binding_id == binding_id) {
          return {&block, &field};
        }
      }
    }
    for (const auto &block : layout.pipeline_layout.value_blocks) {
      for (const auto &field : block.fields) {
        if (field.binding_id == binding_id) {
          return {&block, &field};
        }
      }
    }
    return {nullptr, nullptr};
  };

  for (const auto &value : binding_group->values) {
    auto [owner_block, owner_field] = find_block_for_field(value.binding_id);
    if (owner_block == nullptr || owner_field == nullptr) {
      continue;
    }

    const uint32_t block_set = owner_block->descriptor_set.value_or(0);
    if (block_set != set) {
      continue;
    }

    auto &pending = pending_blocks[owner_block->block_id];
    if (pending.block == nullptr) {
      pending.block = owner_block;
      pending.ubo_data.resize(owner_block->size, 0);
    }

    const size_t copy_size = std::min(
        value.bytes.size(), static_cast<size_t>(owner_field->size)
    );
    if (owner_field->offset + copy_size <= pending.ubo_data.size()) {
      std::memcpy(
          pending.ubo_data.data() + owner_field->offset,
          value.bytes.data(),
          copy_size
      );
    }
  }

  uint64_t content_hash = 0;
  for (const auto &[block_id, pending] : pending_blocks) {
    (void)block_id;
    const VkDeviceSize ubo_size = static_cast<VkDeviceSize>(pending.ubo_data.size());
    const VkDeviceSize ubo_alignment = std::max<VkDeviceSize>(
        1,
        m_device->physical_device_properties()
            .limits.minUniformBufferOffsetAlignment
    );
    auto allocation = allocate_upload_allocation(ubo_size, ubo_alignment);
    std::memcpy(
        allocation.mapped, pending.ubo_data.data(), pending.ubo_data.size()
    );

    buffer_infos.push_back(VkDescriptorBufferInfo{
        .buffer = allocation.buffer,
        .offset = allocation.offset,
        .range = ubo_size,
    });

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = pending.block->binding.value_or(0);
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &buffer_infos.back();
    writes.push_back(write);

    hash_append_value(content_hash, write.dstBinding);
    hash_append_bytes(
        content_hash,
        pending.ubo_data.data(),
        pending.ubo_data.size()
    );
  }

  std::set<uint32_t> written_bindings;
  for (const auto &write : writes) {
    written_bindings.insert(write.dstBinding);
  }

  for (const auto &image_binding : binding_group->sampled_images) {
    const ShaderResourceBindingDesc *resource =
        m_bound_program->resource_binding(image_binding.binding_id);
    if (resource == nullptr || resource->descriptor_set != set) {
      continue;
    }

    const std::optional<VkImageViewType> preferred_view_type =
        image_binding.target == CompiledSampledImageTarget::TextureCube
            ? std::optional<VkImageViewType>{VK_IMAGE_VIEW_TYPE_CUBE}
            : std::nullopt;
    auto image = resolve_image_view(
        image_binding.view, false, preferred_view_type
    );
    if (!image.valid()) {
      continue;
    }

    const VkImageLayout sampled_layout =
        sampled_layout_for_aspect(image.aspect);
    transition_image(command_buffer, image, sampled_layout);
    const VkSampler sampler = sampler_for_format(image.format, image.aspect);

    image_infos.push_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = image.view,
        .imageLayout = sampled_layout,
    });

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = resource->binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image_infos.back();
    writes.push_back(write);
    written_bindings.insert(resource->binding);

    hash_append_value(content_hash, resource->binding);
    hash_append_bytes(content_hash, &sampler, sizeof(sampler));
    hash_append_bytes(content_hash, &image.view, sizeof(image.view));
    hash_append_value(content_hash, sampled_layout);
  }

  ensure_default_images_initialized(command_buffer);

  for (const auto &resource : layout.resource_layout.resources) {
    if (resource.descriptor_set != set || written_bindings.count(resource.binding)) {
      continue;
    }

    if (resource.source_kind == ShaderResourceBindingKind::Sampler) {
      const auto image = resolve_default_sampler_image(resource);
      if (!image.valid()) {
        continue;
      }

      const VkSampler sampler = sampler_for_format(image.format, image.aspect);
      const VkImageLayout sampled_layout =
          sampled_layout_for_aspect(image.aspect);
      image_infos.push_back(VkDescriptorImageInfo{
          .sampler = sampler,
          .imageView = image.view,
          .imageLayout = sampled_layout,
      });

      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstBinding = resource.binding;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      write.pImageInfo = &image_infos.back();
      writes.push_back(write);
      written_bindings.insert(resource.binding);

      hash_append_value(content_hash, resource.binding);
      hash_append_bytes(content_hash, &sampler, sizeof(sampler));
      hash_append_bytes(content_hash, &image.view, sizeof(image.view));
      hash_append_value(content_hash, sampled_layout);
    } else if (resource.source_kind ==
               ShaderResourceBindingKind::StorageBuffer) {
      constexpr VkDeviceSize dummy_size = 256;
      const VkDeviceSize storage_alignment = std::max<VkDeviceSize>(
          1,
          m_device->physical_device_properties()
              .limits.minStorageBufferOffsetAlignment
      );
      auto allocation = allocate_upload_allocation(dummy_size, storage_alignment);
      std::memset(allocation.mapped, 0, static_cast<size_t>(dummy_size));

      buffer_infos.push_back(VkDescriptorBufferInfo{
          .buffer = allocation.buffer,
          .offset = allocation.offset,
          .range = dummy_size,
      });

      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstBinding = resource.binding;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      write.pBufferInfo = &buffer_infos.back();
      writes.push_back(write);
      written_bindings.insert(resource.binding);

      hash_append_value(content_hash, resource.binding);
      hash_append_value(content_hash, dummy_size);
    }
  }

  const bool can_cache = should_cache_binding_group(*binding_group);
  VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
  if (can_cache) {
    const auto frame_index = m_frame_context->current_frame_index();
    DescriptorSetCacheKey cache_key{
        .layout_key = binding_group->layout_key,
        .reuse_identity = binding_group->reuse_identity,
        .content_hash = content_hash,
    };
    const auto cached =
        m_descriptor_set_cache[frame_index].find(cache_key);
    if (cached != m_descriptor_set_cache[frame_index].end()) {
      descriptor_set = cached->second;
    } else {
      descriptor_set = m_descriptor_allocator->allocate_set(set_layout);
      if (descriptor_set == VK_NULL_HANDLE) {
        return;
      }

      for (auto &write : writes) {
        write.dstSet = descriptor_set;
      }
      if (!writes.empty()) {
        vkUpdateDescriptorSets(
            m_device->logical_device(),
            static_cast<uint32_t>(writes.size()),
            writes.data(),
            0,
            nullptr
        );
      }
      m_descriptor_set_cache[frame_index].emplace(cache_key, descriptor_set);
    }
  } else {
    descriptor_set = m_descriptor_allocator->allocate_set(set_layout);
    if (descriptor_set == VK_NULL_HANDLE) {
      return;
    }

    for (auto &write : writes) {
      write.dstSet = descriptor_set;
    }
    if (!writes.empty()) {
      vkUpdateDescriptorSets(
          m_device->logical_device(),
          static_cast<uint32_t>(writes.size()),
          writes.data(),
          0,
          nullptr
      );
    }
  }

  vkCmdBindDescriptorSets(
      command_buffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_bound_pipeline_layout,
      set,
      1,
      &descriptor_set,
      0,
      nullptr
  );
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const BindVertexBufferCmd &command) {
  const CompiledBuffer *compiled_buffer = m_frame->find_buffer(command.buffer);
  if (!compiled_buffer) {
    LOG_WARN("[Vulkan] BindVertexBuffer: unknown buffer handle {}", command.buffer.id);
    return;
  }

  const auto &vertex_bytes = compiled_buffer->is_transient
                                 ? compiled_buffer->transient_data
                                 : compiled_buffer->persistent_vertex_data;
  if (vertex_bytes.empty()) {
    LOG_WARN("[Vulkan] BindVertexBuffer: buffer '{}' has no vertex data", compiled_buffer->debug_name);
    return;
  }

  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceSize base_offset = 0;
  if (compiled_buffer->is_transient) {
    VkDeviceSize data_size = static_cast<VkDeviceSize>(vertex_bytes.size());
    auto allocation = allocate_upload_allocation(data_size);
    std::memcpy(allocation.mapped, vertex_bytes.data(), data_size);
    buffer = allocation.buffer;
    base_offset = allocation.offset;
  } else {
    auto [it, inserted] = m_uploaded_vertex_buffers.try_emplace(
        command.buffer.id, UploadedBufferBinding{}
    );
    if (inserted || it->second.buffer == VK_NULL_HANDLE) {
      VkDeviceSize data_size = static_cast<VkDeviceSize>(vertex_bytes.size());
      auto allocation = allocate_upload_allocation(data_size);
      std::memcpy(allocation.mapped, vertex_bytes.data(), data_size);
      it->second = UploadedBufferBinding{
          .buffer = allocation.buffer,
          .offset = allocation.offset,
      };
    }
    buffer = it->second.buffer;
    base_offset = it->second.offset;
  }

  VkDeviceSize offset = base_offset + command.offset;
  vkCmdBindVertexBuffers(command_buffer, command.slot, 1, &buffer, &offset);

  if (command.slot == 0) {
    if (const BufferLayout *layout = compiled_buffer_layout(*compiled_buffer);
        layout != nullptr) {
      m_bound_vertex_layout = *layout;
      m_has_bound_vertex_layout = true;
    } else {
      m_bound_vertex_layout = {};
      m_has_bound_vertex_layout = false;
    }

    try_bind_current_pipeline(command_buffer);
  }
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const BindIndexBufferCmd &command) {
  if (m_bound_pipeline == VK_NULL_HANDLE &&
      !try_bind_current_pipeline(command_buffer)) {
    return;
  }
  const CompiledBuffer *compiled_buffer = m_frame->find_buffer(command.buffer);
  if (!compiled_buffer) {
    LOG_WARN("[Vulkan] BindIndexBuffer: unknown buffer handle {}", command.buffer.id);
    return;
  }

  const auto &index_bytes = compiled_buffer->persistent_index_data;
  if (index_bytes.empty()) {
    LOG_WARN("[Vulkan] BindIndexBuffer: buffer '{}' has no index data", compiled_buffer->debug_name);
    return;
  }

  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceSize base_offset = 0;
  if (compiled_buffer->is_transient) {
    VkDeviceSize data_size = static_cast<VkDeviceSize>(index_bytes.size());
    auto allocation = allocate_upload_allocation(data_size);
    std::memcpy(allocation.mapped, index_bytes.data(), data_size);
    buffer = allocation.buffer;
    base_offset = allocation.offset;
  } else {
    auto [it, inserted] = m_uploaded_index_buffers.try_emplace(
        command.buffer.id, UploadedBufferBinding{}
    );
    if (inserted || it->second.buffer == VK_NULL_HANDLE) {
      VkDeviceSize data_size = static_cast<VkDeviceSize>(index_bytes.size());
      auto allocation = allocate_upload_allocation(data_size);
      std::memcpy(allocation.mapped, index_bytes.data(), data_size);
      it->second = UploadedBufferBinding{
          .buffer = allocation.buffer,
          .offset = allocation.offset,
      };
    }
    buffer = it->second.buffer;
    base_offset = it->second.offset;
  }

  VkIndexType index_type = (command.index_type == IndexType::Uint16)
                               ? VK_INDEX_TYPE_UINT16
                               : VK_INDEX_TYPE_UINT32;
  vkCmdBindIndexBuffer(command_buffer, buffer, base_offset + command.offset, index_type);
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const DrawIndexedCmd &command) {
  if (m_bound_pipeline == VK_NULL_HANDLE &&
      !try_bind_current_pipeline(command_buffer)) {
    return;
  }
  vkCmdDrawIndexed(command_buffer, command.args.index_count, command.args.instance_count, command.args.first_index, command.args.vertex_offset, command.args.first_instance);
}

void VulkanExecutor::dispatch(
    VkCommandBuffer command_buffer,
    const DispatchComputeCmd &command
) {
  (void)command_buffer;
  (void)command;
  ASTRA_EXCEPTION("Vulkan compute dispatch is not implemented yet");
}

void VulkanExecutor::dispatch(
    VkCommandBuffer command_buffer,
    const MemoryBarrierCmd &command
) {
  (void)command_buffer;
  (void)command;
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const DrawVerticesCmd &command) {
  if (m_bound_pipeline == VK_NULL_HANDLE &&
      !try_bind_current_pipeline(command_buffer)) {
    return;
  }
  vkCmdDraw(command_buffer, command.vertex_count, 1, command.first_vertex, 0);
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const CopyImageCmd &command) {
  auto source = resolve_image_handle(command.src);
  auto destination = resolve_image_handle(command.dst);
  if (!source.valid() || !destination.valid()) {
    return;
  }

  transition_image(command_buffer, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  transition_image(command_buffer, destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  const uint32_t width = command.region.width != 0
                             ? command.region.width
                             : std::min(source.extent.width, destination.extent.width);
  const uint32_t height = command.region.height != 0
                              ? command.region.height
                              : std::min(source.extent.height, destination.extent.height);

  VkImageCopy region{};
  region.srcSubresource.aspectMask = source.aspect;
  region.srcSubresource.mipLevel = command.region.src_mip;
  region.srcSubresource.baseArrayLayer = command.region.src_layer;
  region.srcSubresource.layerCount = 1;
  region.dstSubresource.aspectMask = destination.aspect;
  region.dstSubresource.mipLevel = command.region.dst_mip;
  region.dstSubresource.baseArrayLayer = command.region.dst_layer;
  region.dstSubresource.layerCount = 1;
  region.extent = {width, height, command.region.depth};

  vkCmdCopyImage(
      command_buffer,
      source.image,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      destination.image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &region
  );
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const ResolveImageCmd &command) {
  auto source = resolve_image_handle(command.src);
  auto destination = resolve_image_handle(command.dst);
  if (!source.valid() || !destination.valid()) {
    return;
  }

  transition_image(command_buffer, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  transition_image(command_buffer, destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  if (source.owned_image != nullptr &&
      source.owned_image->samples() != VK_SAMPLE_COUNT_1_BIT) {
    VkImageResolve region{};
    region.srcSubresource.aspectMask = source.aspect;
    region.srcSubresource.layerCount = 1;
    region.dstSubresource.aspectMask = destination.aspect;
    region.dstSubresource.layerCount = 1;
    region.extent = {
        std::min(source.extent.width, destination.extent.width),
        std::min(source.extent.height, destination.extent.height),
        1,
    };

    vkCmdResolveImage(
        command_buffer,
        source.image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        destination.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );
    return;
  }

  dispatch(command_buffer, CopyImageCmd{
                               .src = command.src,
                               .dst = command.dst,
                               .region = CopyRegion::full(ImageExtent{
                                   .width = std::min(source.extent.width, destination.extent.width),
                                   .height = std::min(source.extent.height, destination.extent.height),
                                   .depth = 1,
                               }),
                           });
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const ReadbackImageCmd &command) {
  if (command.out_value == nullptr) {
    return;
  }

  if (command.out_ready != nullptr) {
    *command.out_ready = true;
  }
  *command.out_value = 0;

  const auto *compiled_image = m_frame != nullptr ? m_frame->find_image(command.src)
                                                  : nullptr;
  if (compiled_image == nullptr) {
    LOG_WARN("[Vulkan] ReadbackImageCmd: unknown image handle {}", command.src.id);
    return;
  }
  if (compiled_image->source == CompiledImageSourceKind::DefaultColorTarget) {
    LOG_WARN("[Vulkan] ReadbackImageCmd: swapchain images are not valid readback sources");
    return;
  }
  if (command.x < 0 || command.y < 0 ||
      command.x >= static_cast<int>(compiled_image->extent.width) ||
      command.y >= static_cast<int>(compiled_image->extent.height)) {
    LOG_WARN(
        "[Vulkan] ReadbackImageCmd: pixel ({}, {}) is out of bounds for '{}' ({}x{})",
        command.x,
        command.y,
        compiled_image->debug_name,
        compiled_image->extent.width,
        compiled_image->extent.height
    );
    return;
  }

  auto source = resolve_image_handle(command.src);
  if (!source.valid()) {
    return;
  }
  if (source.owned_image != nullptr &&
      source.owned_image->samples() != VK_SAMPLE_COUNT_1_BIT) {
    LOG_WARN(
        "[Vulkan] ReadbackImageCmd: multisampled image '{}' is not a valid readback source",
        compiled_image->debug_name
    );
    return;
  }

  transition_image(command_buffer, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  auto readback_buffer = std::make_unique<VulkanBuffer>(
      *m_device,
      sizeof(int32_t),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = source.aspect;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {command.x, command.y, 0};
  region.imageExtent = {1, 1, 1};

  vkCmdCopyImageToBuffer(
      command_buffer,
      source.image,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      readback_buffer->handle(),
      1,
      &region
  );

  if (command.out_ready != nullptr) {
    *command.out_ready = false;
  }

  m_pending_readbacks[m_frame_context->current_frame_index()].push_back(
      PendingReadback{
          .buffer = std::move(readback_buffer),
          .out_value = command.out_value,
          .out_ready = command.out_ready,
          .format = source.format,
      }
  );
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const SetScissorCmd &command) {
  if (!command.enabled) {
    VkRect2D full_scissor{};
    full_scissor.offset = {0, 0};
    full_scissor.extent = m_active_render_extent;
    vkCmdSetScissor(command_buffer, 0, 1, &full_scissor);
    return;
  }

  VkRect2D scissor{};
  scissor.offset = {static_cast<int32_t>(command.x), static_cast<int32_t>(command.y)};
  scissor.extent = {command.width, command.height};
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

void VulkanExecutor::dispatch(VkCommandBuffer command_buffer, const SetViewportCmd &command) {
  VkViewport viewport{};
  viewport.x = static_cast<float>(command.x);
  viewport.y = static_cast<float>(command.y);
  viewport.width = static_cast<float>(command.width);
  viewport.height = static_cast<float>(command.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {static_cast<int32_t>(command.x), static_cast<int32_t>(command.y)};
  scissor.extent = {command.width, command.height};
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

std::optional<int> VulkanExecutor::read_pixel(
    const CompiledFrame &frame, ImageHandle src, int x, int y
) const {
  const auto *image = frame.find_image(src);
  if (image == nullptr ||
      image->source == CompiledImageSourceKind::DefaultColorTarget) {
    return std::nullopt;
  }
  if (x < 0 || y < 0 || x >= static_cast<int>(image->extent.width) ||
      y >= static_cast<int>(image->extent.height)) {
    return std::nullopt;
  }

  const CompiledFrame *previous_frame = m_frame;
  const_cast<VulkanExecutor *>(this)->m_frame = &frame;
  auto resolved = const_cast<VulkanExecutor *>(this)->resolve_image_handle(src);
  const_cast<VulkanExecutor *>(this)->m_frame = previous_frame;

  if (resolved.image == VK_NULL_HANDLE ||
      (resolved.owned_image != nullptr &&
       resolved.owned_image->samples() != VK_SAMPLE_COUNT_1_BIT)) {
    return std::nullopt;
  }

  VkDevice device = m_device->logical_device();
  VkQueue queue = m_device->graphics_queue();

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  pool_info.queueFamilyIndex =
      m_device->queue_families().graphics_family.value();

  VkCommandPool transient_pool = VK_NULL_HANDLE;
  if (vkCreateCommandPool(device, &pool_info, nullptr, &transient_pool) !=
      VK_SUCCESS) {
    return std::nullopt;
  }

  VkCommandBufferAllocateInfo alloc_command_info{};
  alloc_command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_command_info.commandPool = transient_pool;
  alloc_command_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_command_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(device, &alloc_command_info, &command_buffer) !=
      VK_SUCCESS) {
    vkDestroyCommandPool(device, transient_pool, nullptr);
    return std::nullopt;
  }

  VulkanBuffer staging_buffer(
      *m_device, sizeof(int32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(command_buffer, &begin_info);

  const VkImageLayout previous_layout = tracked_layout_for(resolved);
  const_cast<VulkanExecutor *>(this)->transition_image(
      command_buffer, resolved, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  );

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = resolved.aspect;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {x, y, 0};
  region.imageExtent = {1, 1, 1};

  vkCmdCopyImageToBuffer(command_buffer, resolved.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buffer.handle(), 1, &region);

  if (previous_layout != VK_IMAGE_LAYOUT_UNDEFINED) {
    const_cast<VulkanExecutor *>(this)->transition_image(
        command_buffer, resolved, previous_layout
    );
  }

  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);

  int32_t pixel_value = 0;
  staging_buffer.map();
  std::memcpy(&pixel_value, staging_buffer.mapped(), sizeof(int32_t));
  staging_buffer.unmap();

  vkDestroyCommandPool(device, transient_pool, nullptr);

  return static_cast<int>(pixel_value);
}

} // namespace astralix
