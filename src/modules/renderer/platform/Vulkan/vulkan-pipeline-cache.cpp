#include "vulkan-pipeline-cache.hpp"
#include "assert.hpp"
#include "fnv1a.hpp"
#include "vulkan-device.hpp"
#include "vulkan-shader-program.hpp"
#include <unordered_set>

namespace astralix {

namespace {

VkCullModeFlags to_vulkan_cull_mode(CullMode mode) {
  switch (mode) {
    case CullMode::None:
      return VK_CULL_MODE_NONE;
    case CullMode::Front:
      return VK_CULL_MODE_FRONT_BIT;
    case CullMode::Back:
      return VK_CULL_MODE_BACK_BIT;
    default:
      return VK_CULL_MODE_NONE;
  }
}

VkFrontFace to_vulkan_front_face(FrontFace face) {
  switch (face) {
    case FrontFace::Clockwise:
      return VK_FRONT_FACE_CLOCKWISE;
    case FrontFace::CounterClockwise:
      return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    default:
      return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  }
}

VkCompareOp to_vulkan_compare_op(CompareOp op) {
  switch (op) {
    case CompareOp::Never:
      return VK_COMPARE_OP_NEVER;
    case CompareOp::Less:
      return VK_COMPARE_OP_LESS;
    case CompareOp::LessEqual:
      return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareOp::Equal:
      return VK_COMPARE_OP_EQUAL;
    case CompareOp::Greater:
      return VK_COMPARE_OP_GREATER;
    case CompareOp::Always:
      return VK_COMPARE_OP_ALWAYS;
    default:
      return VK_COMPARE_OP_LESS;
  }
}

VkBlendFactor to_vulkan_blend_factor(BlendFactor factor) {
  switch (factor) {
    case BlendFactor::Zero:
      return VK_BLEND_FACTOR_ZERO;
    case BlendFactor::One:
      return VK_BLEND_FACTOR_ONE;
    case BlendFactor::SrcAlpha:
      return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    default:
      return VK_BLEND_FACTOR_ONE;
  }
}

VkFormat shader_data_type_to_vulkan_format(ShaderDataType type) {
  switch (type) {
    case ShaderDataType::Float:
      return VK_FORMAT_R32_SFLOAT;
    case ShaderDataType::Float2:
      return VK_FORMAT_R32G32_SFLOAT;
    case ShaderDataType::Float3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case ShaderDataType::Float4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case ShaderDataType::Mat3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case ShaderDataType::Mat4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case ShaderDataType::Int:
      return VK_FORMAT_R32_SINT;
    case ShaderDataType::Int2:
      return VK_FORMAT_R32G32_SINT;
    case ShaderDataType::Int3:
      return VK_FORMAT_R32G32B32_SINT;
    case ShaderDataType::Int4:
      return VK_FORMAT_R32G32B32A32_SINT;
    case ShaderDataType::Bool:
      return VK_FORMAT_R8_UINT;
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

uint32_t shader_data_type_location_span(ShaderDataType type) {
  switch (type) {
    case ShaderDataType::Mat3:
      return 3;
    case ShaderDataType::Mat4:
      return 4;
    default:
      return 1;
  }
}

uint32_t matrix_column_size_bytes(ShaderDataType type) {
  switch (type) {
    case ShaderDataType::Mat3:
      return sizeof(float) * 3;
    case ShaderDataType::Mat4:
      return sizeof(float) * 4;
    default:
      return 0;
  }
}

std::vector<VkVertexInputAttributeDescription> build_attribute_descriptions(
    const BufferLayout *vertex_layout,
    const VertexInputLayoutDesc &vertex_input
) {
  std::vector<VkVertexInputAttributeDescription> attribute_descs;
  if (vertex_layout == nullptr) {
    return attribute_descs;
  }

  std::unordered_set<uint32_t> used_locations;
  for (const auto &attribute : vertex_input.attributes) {
    used_locations.insert(attribute.location);
  }

  uint32_t next_location = 0;
  for (const auto &element : vertex_layout->get_elements()) {
    const uint32_t base_location =
        element.has_explicit_location() ? element.location : next_location;
    const uint32_t span = shader_data_type_location_span(element.type);

    if (element.type == ShaderDataType::Mat3 ||
        element.type == ShaderDataType::Mat4) {
      const uint32_t column_size = matrix_column_size_bytes(element.type);
      for (uint32_t column = 0; column < span; ++column) {
        if (!used_locations.count(base_location + column)) {
          continue;
        }
        VkVertexInputAttributeDescription attr{};
        attr.location = base_location + column;
        attr.binding = 0;
        attr.format = shader_data_type_to_vulkan_format(element.type);
        attr.offset =
            static_cast<uint32_t>(element.offset) + column_size * column;
        attribute_descs.push_back(attr);
      }
    } else {
      if (!used_locations.count(base_location)) {
        next_location = base_location + span;
        continue;
      }
      VkVertexInputAttributeDescription attr{};
      attr.location = base_location;
      attr.binding = 0;
      attr.format = shader_data_type_to_vulkan_format(element.type);
      attr.offset = static_cast<uint32_t>(element.offset);
      attribute_descs.push_back(attr);
    }

    next_location = base_location + span;
  }

  return attribute_descs;
}

void append_vertex_layout_to_hash(uint64_t &hash, const BufferLayout *vertex_layout) {
  if (vertex_layout == nullptr) {
    hash = fnv1a64_append_value(hash, uint64_t{0});
    return;
  }

  hash = fnv1a64_append_value(
      hash, static_cast<uint64_t>(vertex_layout->get_stride())
  );
  hash = fnv1a64_append_value(
      hash, static_cast<uint64_t>(vertex_layout->get_elements().size())
  );

  for (const auto &element : vertex_layout->get_elements()) {
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(element.type));
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(element.size));
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(element.offset));
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(element.location));
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(element.normalized));
  }
}

} // namespace

VulkanPipelineCache::VulkanPipelineCache(const VulkanDevice &device)
    : m_device(device) {
  VkPipelineCacheCreateInfo cache_info{};
  cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  vkCreatePipelineCache(device.logical_device(), &cache_info, nullptr, &m_vk_cache);
}

VulkanPipelineCache::~VulkanPipelineCache() {
  VkDevice device = m_device.logical_device();
  for (auto &[key, pipeline] : m_pipelines)
    vkDestroyPipeline(device, pipeline, nullptr);
  if (m_vk_cache != VK_NULL_HANDLE)
    vkDestroyPipelineCache(device, m_vk_cache, nullptr);
}

VkPipeline VulkanPipelineCache::get_or_create_graphics_pipeline(
    const VulkanShaderProgram &program, const RenderPipelineDesc &desc,
    const BufferLayout *vertex_layout, const std::vector<VkFormat> &color_formats,
    VkFormat depth_format
) {
  uint64_t key =
      compute_pipeline_key(program, desc, vertex_layout, color_formats, depth_format);
  auto it = m_pipelines.find(key);
  if (it != m_pipelines.end())
    return it->second;

  VkPipeline pipeline =
      create_graphics_pipeline(program, desc, vertex_layout, color_formats, depth_format);
  m_pipelines[key] = pipeline;
  return pipeline;
}

uint64_t VulkanPipelineCache::compute_pipeline_key(
    const VulkanShaderProgram &program, const RenderPipelineDesc &desc,
    const BufferLayout *vertex_layout, const std::vector<VkFormat> &color_formats,
    VkFormat depth_format
) const {
  uint64_t hash = k_fnv1a64_offset_basis;
  hash = fnv1a64_append_value(hash, reinterpret_cast<uintptr_t>(&program));
  append_vertex_layout_to_hash(hash, vertex_layout);
  for (VkFormat color_format : color_formats) {
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(color_format));
  }
  hash = fnv1a64_append_value(hash, static_cast<uint64_t>(color_formats.size()));
  hash = fnv1a64_append_value(hash, static_cast<uint64_t>(depth_format));
  hash = fnv1a64_append_value(hash, static_cast<uint64_t>(desc.raster.cull_mode));
  hash = fnv1a64_append_value(hash, static_cast<uint64_t>(desc.raster.front_face));
  hash = fnv1a64_append_value(hash, static_cast<uint64_t>(desc.depth_stencil.depth_test));
  hash = fnv1a64_append_value(hash, static_cast<uint64_t>(desc.depth_stencil.depth_write));
  hash = fnv1a64_append_value(hash, static_cast<uint64_t>(desc.depth_stencil.compare_op));
  for (const auto &blend : desc.blend_attachments) {
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(blend.enabled));
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(blend.src));
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(blend.dst));
  }
  return hash;
}

VkPipeline VulkanPipelineCache::create_graphics_pipeline(
    const VulkanShaderProgram &program, const RenderPipelineDesc &desc,
    const BufferLayout *vertex_layout, const std::vector<VkFormat> &color_formats,
    VkFormat depth_format
) {
  auto stage_infos = program.stage_create_infos();
  const auto &vertex_input = program.vertex_input();

  VkVertexInputBindingDescription binding_desc{};
  binding_desc.binding = 0;
  binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  binding_desc.stride =
      vertex_layout != nullptr ? vertex_layout->get_stride() : 0;

  auto attribute_descs =
      build_attribute_descriptions(vertex_layout, vertex_input);

  VkPipelineVertexInputStateCreateInfo vertex_input_state{};
  vertex_input_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  if (vertex_layout != nullptr && !attribute_descs.empty()) {
    vertex_input_state.vertexBindingDescriptionCount = 1;
    vertex_input_state.pVertexBindingDescriptions = &binding_desc;
    vertex_input_state.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attribute_descs.size());
    vertex_input_state.pVertexAttributeDescriptions = attribute_descs.data();
  }

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterization{};
  rasterization.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization.cullMode = to_vulkan_cull_mode(desc.raster.cull_mode);
  rasterization.frontFace = to_vulkan_front_face(desc.raster.front_face);
  rasterization.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depth_stencil{};
  depth_stencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable =
      desc.depth_stencil.depth_test ? VK_TRUE : VK_FALSE;
  depth_stencil.depthWriteEnable =
      desc.depth_stencil.depth_write ? VK_TRUE : VK_FALSE;
  depth_stencil.depthCompareOp =
      to_vulkan_compare_op(desc.depth_stencil.compare_op);

  std::vector<VkPipelineColorBlendAttachmentState> blend_attachments;
  const uint32_t attachment_count =
      std::max<uint32_t>(1, static_cast<uint32_t>(color_formats.size()));
  if (desc.blend_attachments.empty()) {
    blend_attachments.resize(attachment_count);
    for (auto &state : blend_attachments) {
      state.colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }
  } else {
    for (uint32_t attachment_index = 0; attachment_index < attachment_count;
         ++attachment_index) {
      const auto &blend = desc.blend_attachments[std::min<size_t>(attachment_index, desc.blend_attachments.size() - 1)];
      VkPipelineColorBlendAttachmentState state{};
      state.colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      state.blendEnable = blend.enabled ? VK_TRUE : VK_FALSE;
      if (blend.enabled) {
        state.srcColorBlendFactor = to_vulkan_blend_factor(blend.src);
        state.dstColorBlendFactor = to_vulkan_blend_factor(blend.dst);
        state.colorBlendOp = VK_BLEND_OP_ADD;
        state.srcAlphaBlendFactor = to_vulkan_blend_factor(blend.src);
        state.dstAlphaBlendFactor = to_vulkan_blend_factor(blend.dst);
        state.alphaBlendOp = VK_BLEND_OP_ADD;
      }
      blend_attachments.push_back(state);
    }
  }

  VkPipelineColorBlendStateCreateInfo color_blend{};
  color_blend.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.attachmentCount =
      static_cast<uint32_t>(blend_attachments.size());
  color_blend.pAttachments = blend_attachments.data();

  std::array<VkDynamicState, 2> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount =
      static_cast<uint32_t>(dynamic_states.size());
  dynamic_state.pDynamicStates = dynamic_states.data();

  VkPipelineRenderingCreateInfo rendering_info{};
  rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering_info.colorAttachmentCount =
      static_cast<uint32_t>(color_formats.size());
  rendering_info.pColorAttachmentFormats = color_formats.data();
  if (depth_format != VK_FORMAT_UNDEFINED) {
    rendering_info.depthAttachmentFormat = depth_format;

    const bool has_stencil =
        depth_format == VK_FORMAT_D24_UNORM_S8_UINT ||
        depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        depth_format == VK_FORMAT_D16_UNORM_S8_UINT ||
        depth_format == VK_FORMAT_S8_UINT;
    rendering_info.stencilAttachmentFormat =
        has_stencil ? depth_format : VK_FORMAT_UNDEFINED;
  }

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.pNext = &rendering_info;
  pipeline_info.stageCount = static_cast<uint32_t>(stage_infos.size());
  pipeline_info.pStages = stage_infos.data();
  pipeline_info.pVertexInputState = &vertex_input_state;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterization;
  pipeline_info.pMultisampleState = &multisample;
  pipeline_info.pDepthStencilState = &depth_stencil;
  pipeline_info.pColorBlendState = &color_blend;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = program.pipeline_layout();

  VkPipeline pipeline = VK_NULL_HANDLE;
  VkResult result = vkCreateGraphicsPipelines(
      m_device.logical_device(), m_vk_cache, 1, &pipeline_info, nullptr, &pipeline
  );
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create graphics pipeline");

  return pipeline;
}

} // namespace astralix
