#pragma once

#include "assert.hpp"
#include "render-ir.hpp"
#include "render-types.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-image-export.hpp"
#include "vertex-array.hpp"
#include "vertex-buffer.hpp"
#include "virtual-index-buffer.hpp"
#include "virtual-vertex-buffer.hpp"
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace astralix {

struct CompiledPass {
  std::string debug_name;
  RenderGraphPassType type = RenderGraphPassType::Graphics;
  std::vector<uint32_t> dependency_pass_indices;
  RenderCommandBuffer commands;
};

enum class CompiledImageSourceKind : uint8_t {
  DefaultColorTarget,
  GraphImage,
  Texture2DResource,
  TextureCubeResource,
  RawTextureId,
};

struct CompiledImage {
  std::string debug_name;
  CompiledImageSourceKind source = CompiledImageSourceKind::DefaultColorTarget;
  std::shared_ptr<RenderGraphImageResource> graph_image;
  Ref<Texture> texture = nullptr;
  ImageAspect aspect = ImageAspect::Color0;
  uint32_t raw_renderer_id = 0;
  ImageExtent extent{};
};

struct CompiledPipeline {
  std::string debug_name;
  RenderPipelineDesc desc;
  Ref<Shader> shader;
  void *vulkan_program = nullptr;
  std::string shader_descriptor_id;
};

struct CompiledValueBinding {
  uint64_t binding_id = 0;
  ShaderValueKind kind = ShaderValueKind::Float;
  std::vector<uint8_t> bytes;
};

enum class CompiledSampledImageTarget : uint8_t { Texture2D,
                                                  TextureCube };

struct CompiledSampledImageBinding {
  uint64_t binding_id = 0;
  ImageViewRef view{};
  CompiledSampledImageTarget target = CompiledSampledImageTarget::Texture2D;
};

struct BindingGroupDesc {
  std::string debug_name;
  std::string owner_pass;
  RenderBindingLayoutKey layout_key;
  RenderBindingReuseIdentity reuse_identity;
  RenderBindingScope scope = RenderBindingScope::Draw;
  RenderBindingCachePolicy cache_policy = RenderBindingCachePolicy::Auto;
  RenderBindingStability stability = RenderBindingStability::FrameLocal;
};

struct CompiledBindingGroup {
  std::string debug_name;
  std::string owner_pass;
  RenderBindingLayoutKey layout_key;
  RenderBindingReuseIdentity reuse_identity;
  RenderBindingScope scope = RenderBindingScope::Draw;
  RenderBindingCachePolicy cache_policy = RenderBindingCachePolicy::Auto;
  RenderBindingStability stability = RenderBindingStability::FrameLocal;
  std::vector<CompiledValueBinding> values;
  std::vector<CompiledSampledImageBinding> sampled_images;
};

inline BindingGroupDesc make_binding_group_desc(
    std::string debug_name,
    std::string owner_pass,
    Ref<Shader> shader,
    uint32_t descriptor_set_index,
    std::string cache_namespace,
    RenderBindingScope scope = RenderBindingScope::Draw,
    RenderBindingCachePolicy cache_policy = RenderBindingCachePolicy::Auto,
    RenderBindingSharing sharing = RenderBindingSharing::LocalOnly,
    uint64_t stable_tag = 0,
    RenderBindingStability stability = RenderBindingStability::FrameLocal
) {
  if (cache_namespace.empty()) {
    cache_namespace = debug_name;
  }

  return BindingGroupDesc{
      .debug_name = std::move(debug_name),
      .owner_pass = std::move(owner_pass),
      .layout_key = RenderBindingLayoutKey{
          .shader_descriptor_id =
              shader != nullptr ? shader->descriptor_id() : "",
          .descriptor_set_index = descriptor_set_index,
      },
      .reuse_identity = RenderBindingReuseIdentity{
          .sharing = sharing,
          .cache_namespace = std::move(cache_namespace),
          .stable_tag = stable_tag,
      },
      .scope = scope,
      .cache_policy = cache_policy,
      .stability = stability,
  };
}

struct CompiledBuffer {
  std::string debug_name;
  Ref<VertexArray> vertex_array;
  std::vector<uint8_t> transient_data;
  uint32_t transient_vertex_count = 0;
  BufferLayout transient_layout;
  std::vector<uint8_t> persistent_vertex_data;
  BufferLayout persistent_vertex_layout;
  std::vector<uint8_t> persistent_index_data;
  bool is_transient = false;
};

struct CompiledExportEntry {
  RenderImageExportKey key;
  ImageHandle image{};
  ImageExtent extent{};
};

struct CompiledFramePresentEdge {
  ImageViewRef source{};
  ImageExtent extent{};
};

struct CompiledFrame {
  std::vector<CompiledPass> passes;
  std::vector<CompiledImage> images;
  std::vector<CompiledPipeline> pipelines;
  std::vector<CompiledBindingGroup> binding_groups;
  std::vector<CompiledBuffer> buffers;
  std::vector<CompiledFramePresentEdge> present_edges;
  std::vector<CompiledExportEntry> export_entries;

  ImageHandle register_default_color_target(const std::string &debug_name, const ImageExtent &extent) {
    ImageHandle handle{m_next_image_handle_id++};
    images.push_back(CompiledImage{
        .debug_name = debug_name,
        .source = CompiledImageSourceKind::DefaultColorTarget,
        .graph_image = nullptr,
        .texture = nullptr,
        .aspect = ImageAspect::Color0,
        .extent = extent,
    });
    return handle;
  }

  ImageHandle register_graph_image(
      const std::string &debug_name,
      std::shared_ptr<RenderGraphImageResource> graph_image,
      ImageAspect aspect = ImageAspect::Color0
  ) {
    ASTRA_ENSURE(graph_image == nullptr,
                 "Cannot register a null graph image resource");

    ImageHandle handle{m_next_image_handle_id++};
    images.push_back(CompiledImage{
        .debug_name = debug_name,
        .source = CompiledImageSourceKind::GraphImage,
        .graph_image = std::move(graph_image),
        .texture = nullptr,
        .aspect = aspect,
        .extent = images.empty() ? ImageExtent{} : images.back().extent,
    });
    images.back().extent = images.back().graph_image->extent();
    return handle;
  }

  ImageHandle register_texture_2d(const std::string &debug_name, Ref<Texture> texture) {
    ASTRA_ENSURE(texture == nullptr, "Cannot register a null texture-2d image");

    const ImageExtent extent{
        .width = texture->width(),
        .height = texture->height(),
        .depth = 1,
    };

    ImageHandle handle{m_next_image_handle_id++};
    images.push_back(CompiledImage{
        .debug_name = debug_name,
        .source = CompiledImageSourceKind::Texture2DResource,
        .graph_image = nullptr,
        .texture = std::move(texture),
        .aspect = ImageAspect::Color0,
        .extent = extent,
    });
    return handle;
  }

  ImageHandle register_raw_texture_2d(const std::string &debug_name, uint32_t renderer_id, uint32_t width, uint32_t height) {
    ASTRA_ENSURE(renderer_id == 0, "Cannot register a zero raw texture id");

    ImageHandle handle{m_next_image_handle_id++};
    images.push_back(CompiledImage{
        .debug_name = debug_name,
        .source = CompiledImageSourceKind::RawTextureId,
        .graph_image = nullptr,
        .texture = nullptr,
        .aspect = ImageAspect::Color0,
        .raw_renderer_id = renderer_id,
        .extent = ImageExtent{
            .width = width,
            .height = height,
            .depth = 1,
        },
    });
    return handle;
  }

  ImageHandle register_texture_cube(const std::string &debug_name, Ref<Texture> texture) {
    ASTRA_ENSURE(texture == nullptr, "Cannot register a null texture-cube image");

    const ImageExtent extent{
        .width = texture->width(),
        .height = texture->height(),
        .depth = 1,
    };

    ImageHandle handle{m_next_image_handle_id++};
    images.push_back(CompiledImage{
        .debug_name = debug_name,
        .source = CompiledImageSourceKind::TextureCubeResource,
        .graph_image = nullptr,
        .texture = std::move(texture),
        .aspect = ImageAspect::Color0,
        .extent = extent,
    });
    return handle;
  }

  RenderPipelineHandle register_pipeline(const RenderPipelineDesc &desc, Ref<Shader> shader) {
    ASTRA_ENSURE(shader == nullptr, "Cannot register a null render pipeline");

    RenderPipelineHandle handle{m_next_pipeline_handle_id++};
    pipelines.push_back(CompiledPipeline{
        .debug_name = desc.debug_name,
        .desc = desc,
        .shader = shader,
        .shader_descriptor_id = shader->descriptor_id(),
    });
    return handle;
  }

  RenderBindingGroupHandle register_binding_group(const BindingGroupDesc &desc) {
    RenderBindingGroupHandle handle{m_next_binding_group_handle_id++};
    binding_groups.push_back(CompiledBindingGroup{
        .debug_name = desc.debug_name,
        .owner_pass = desc.owner_pass,
        .layout_key = desc.layout_key,
        .reuse_identity = desc.reuse_identity,
        .scope = desc.scope,
        .cache_policy = desc.cache_policy,
        .stability = desc.stability,
    });
    return handle;
  }

  template <typename T>
  void add_value_binding(RenderBindingGroupHandle handle, uint64_t binding_id,
                         ShaderValueKind kind, const T &value) {
    static_assert(std::is_trivially_copyable_v<T>, "Compiled value bindings require trivially copyable payloads");

    auto *binding_group = find_binding_group_mutable(handle);
    ASTRA_ENSURE(binding_group == nullptr, "Unknown binding group handle in compiled frame: ", handle.id);
    ASTRA_ENSURE(binding_id == 0,
                 "Cannot record a value binding with binding id 0");

    CompiledValueBinding binding{
        .binding_id = binding_id,
        .kind = kind,
    };
    binding.bytes.resize(sizeof(T));
    std::memcpy(binding.bytes.data(), &value, sizeof(T));
    binding_group->values.push_back(std::move(binding));
  }

  void add_value_binding_bytes(RenderBindingGroupHandle handle,
                               uint64_t binding_id, ShaderValueKind kind,
                               const std::vector<uint8_t> &bytes) {
    auto *binding_group = find_binding_group_mutable(handle);
    ASTRA_ENSURE(binding_group == nullptr, "Unknown binding group handle in compiled frame: ", handle.id);
    ASTRA_ENSURE(binding_id == 0,
                 "Cannot record a value binding with binding id 0");

    binding_group->values.push_back(CompiledValueBinding{
        .binding_id = binding_id,
        .kind = kind,
        .bytes = bytes,
    });
  }

  void add_sampled_image_binding(
      RenderBindingGroupHandle handle, uint64_t binding_id, ImageViewRef view,
      CompiledSampledImageTarget target = CompiledSampledImageTarget::Texture2D
  ) {
    auto *binding_group = find_binding_group_mutable(handle);
    ASTRA_ENSURE(binding_group == nullptr, "Unknown binding group handle in compiled frame: ", handle.id);
    ASTRA_ENSURE(binding_id == 0,
                 "Cannot record a sampled image binding with binding id 0");

    binding_group->sampled_images.push_back(CompiledSampledImageBinding{
        .binding_id = binding_id,
        .view = view,
        .target = target,
    });
  }

  BufferHandle register_vertex_array(const std::string &debug_name, Ref<VertexArray> vertex_array) {
    ASTRA_ENSURE(vertex_array == nullptr, "Cannot register a null vertex array");

    BufferHandle handle{m_next_buffer_handle_id++};
    CompiledBuffer compiled_buffer{
        .debug_name = debug_name,
        .vertex_array = std::move(vertex_array),
    };

    const auto &vertex_buffers = compiled_buffer.vertex_array->get_vertex_buffers();
    if (!vertex_buffers.empty()) {
      if (const auto *virtual_vertex_buffer =
              dynamic_cast<const VirtualVertexBuffer *>(vertex_buffers.front().get());
          virtual_vertex_buffer != nullptr) {
        compiled_buffer.persistent_vertex_data = virtual_vertex_buffer->bytes();
        compiled_buffer.persistent_vertex_layout = virtual_vertex_buffer->get_layout();
      }
    }

    if (const auto &index_buffer = compiled_buffer.vertex_array->get_index_buffer();
        index_buffer != nullptr) {
      if (const auto *virtual_index_buffer =
              dynamic_cast<const VirtualIndexBuffer *>(index_buffer.get());
          virtual_index_buffer != nullptr) {
        compiled_buffer.persistent_index_data = virtual_index_buffer->bytes();
      }
    }

    buffers.push_back(std::move(compiled_buffer));
    return handle;
  }

  BufferHandle register_transient_vertices(
      const std::string &debug_name, const void *data,
      uint32_t size_bytes, uint32_t vertex_count,
      BufferLayout layout
  ) {
    ASTRA_ENSURE(data == nullptr || size_bytes == 0, "Cannot register empty transient vertex data");

    BufferHandle handle{m_next_buffer_handle_id++};
    CompiledBuffer buffer;
    buffer.debug_name = debug_name;
    buffer.transient_data.resize(size_bytes);
    std::memcpy(buffer.transient_data.data(), data, size_bytes);
    buffer.transient_vertex_count = vertex_count;
    buffer.transient_layout = std::move(layout);
    buffer.is_transient = true;
    buffers.push_back(std::move(buffer));
    return handle;
  }

  const CompiledImage *find_image(ImageHandle handle) const {
    if (!handle.valid()) {
      return nullptr;
    }

    const auto index = static_cast<size_t>(handle.id - 1);
    return index < images.size() ? &images[index] : nullptr;
  }

  const CompiledPipeline *find_pipeline(RenderPipelineHandle handle) const {
    if (!handle.valid()) {
      return nullptr;
    }

    const auto index = static_cast<size_t>(handle.id - 1);
    return index < pipelines.size() ? &pipelines[index] : nullptr;
  }

  const CompiledBindingGroup *
  find_binding_group(RenderBindingGroupHandle handle) const {
    if (!handle.valid()) {
      return nullptr;
    }

    const auto index = static_cast<size_t>(handle.id - 1);
    return index < binding_groups.size() ? &binding_groups[index] : nullptr;
  }

  const CompiledExportEntry *find_export(RenderImageExportKey key) const {
    for (const auto &entry : export_entries) {
      if (entry.key == key) {
        return &entry;
      }
    }
    return nullptr;
  }

  const CompiledBuffer *find_buffer(BufferHandle handle) const {
    if (!handle.valid()) {
      return nullptr;
    }

    const auto index = static_cast<size_t>(handle.id - 1);
    return index < buffers.size() ? &buffers[index] : nullptr;
  }

  void clear() {
    passes.clear();
    images.clear();
    pipelines.clear();
    binding_groups.clear();
    buffers.clear();
    present_edges.clear();
    export_entries.clear();
    m_next_image_handle_id = 1;
    m_next_pipeline_handle_id = 1;
    m_next_binding_group_handle_id = 1;
    m_next_buffer_handle_id = 1;
  }

  [[nodiscard]] bool empty() const noexcept { return passes.empty(); }

private:
  CompiledBindingGroup *find_binding_group_mutable(RenderBindingGroupHandle handle) {
    if (!handle.valid()) {
      return nullptr;
    }

    const auto index = static_cast<size_t>(handle.id - 1);
    return index < binding_groups.size() ? &binding_groups[index] : nullptr;
  }

  uint32_t m_next_image_handle_id = 1;
  uint32_t m_next_pipeline_handle_id = 1;
  uint32_t m_next_binding_group_handle_id = 1;
  uint32_t m_next_buffer_handle_id = 1;
};

} // namespace astralix
