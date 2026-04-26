#include "vulkan-shader-program.hpp"
#include "vulkan-device.hpp"
#include "vulkan-shader.hpp"
#include "assert.hpp"

namespace astralix {

namespace {

VkShaderStageFlagBits to_vulkan_stage(StageKind stage) {
  switch (stage) {
  case StageKind::Vertex:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case StageKind::Fragment:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case StageKind::Geometry:
    return VK_SHADER_STAGE_GEOMETRY_BIT;
  default:
    return VK_SHADER_STAGE_VERTEX_BIT;
  }
}

VkDescriptorType to_descriptor_type(ShaderResourceBindingKind kind) {
  switch (kind) {
  case ShaderResourceBindingKind::Sampler:
    return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  case ShaderResourceBindingKind::UniformBlock:
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  case ShaderResourceBindingKind::StorageBuffer:
    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  default:
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  }
}

} // namespace

VulkanShaderProgram::VulkanShaderProgram(
    const VulkanDevice &device,
    std::map<StageKind, std::vector<uint32_t>> spirv_stages,
    ShaderPipelineLayout layout)
    : m_device(device), m_layout(std::move(layout)) {
  for (auto &[stage, spirv] : spirv_stages) {
    m_modules[stage] = std::make_unique<VulkanShaderModule>(
        device, spirv, to_vulkan_stage(stage));
  }

  create_descriptor_set_layouts();
  create_pipeline_layout();
}

VulkanShaderProgram::~VulkanShaderProgram() {
  VkDevice device = m_device.logical_device();

  if (m_pipeline_layout != VK_NULL_HANDLE)
    vkDestroyPipelineLayout(device, m_pipeline_layout, nullptr);

  for (VkDescriptorSetLayout layout : m_descriptor_set_layouts)
    vkDestroyDescriptorSetLayout(device, layout, nullptr);
}

uint32_t VulkanShaderProgram::stage_count() const noexcept {
  return static_cast<uint32_t>(m_modules.size());
}

std::vector<VkPipelineShaderStageCreateInfo>
VulkanShaderProgram::stage_create_infos() const {
  std::vector<VkPipelineShaderStageCreateInfo> infos;
  infos.reserve(m_modules.size());

  for (const auto &[stage, module] : m_modules) {
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = module->stage();
    info.module = module->handle();
    info.pName = "main";
    infos.push_back(info);
  }

  return infos;
}

VkDescriptorSetLayout
VulkanShaderProgram::descriptor_set_layout(uint32_t set) const {
  if (set < m_descriptor_set_layouts.size())
    return m_descriptor_set_layouts[set];
  return VK_NULL_HANDLE;
}

const ShaderValueBlockDesc *
VulkanShaderProgram::value_block(const std::string &logical_name) const {
  for (const auto &block : m_layout.resource_layout.value_blocks) {
    if (block.logical_name == logical_name)
      return &block;
  }
  for (const auto &block : m_layout.pipeline_layout.value_blocks) {
    if (block.logical_name == logical_name)
      return &block;
  }
  return nullptr;
}

const ShaderResourceBindingDesc *
VulkanShaderProgram::resource_binding(uint64_t binding_id) const {
  for (const auto &resource : m_layout.resource_layout.resources) {
    if (resource.binding_id == binding_id)
      return &resource;
  }
  return nullptr;
}

void VulkanShaderProgram::create_descriptor_set_layouts() {
  uint32_t max_set = 0;
  for (const auto &block : m_layout.resource_layout.value_blocks) {
    uint32_t set = block.descriptor_set.value_or(0);
    if (set > max_set)
      max_set = set;
  }
  for (const auto &resource : m_layout.resource_layout.resources) {
    if (resource.descriptor_set > max_set)
      max_set = resource.descriptor_set;
  }

  m_descriptor_set_layouts.resize(max_set + 1, VK_NULL_HANDLE);

  for (uint32_t set = 0; set <= max_set; ++set) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (const auto &block : m_layout.resource_layout.value_blocks) {
      if (block.descriptor_set.value_or(0) != set)
        continue;
      VkDescriptorSetLayoutBinding binding{};
      binding.binding = block.binding.value_or(0);
      binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      binding.descriptorCount = 1;
      binding.stageFlags = VK_SHADER_STAGE_ALL;
      bindings.push_back(binding);
    }

    for (const auto &resource : m_layout.resource_layout.resources) {
      if (resource.descriptor_set != set)
        continue;
      VkDescriptorSetLayoutBinding binding{};
      binding.binding = resource.binding;
      binding.descriptorType = to_descriptor_type(resource.source_kind);
      binding.descriptorCount = resource.array_size.value_or(1);
      binding.stageFlags = VK_SHADER_STAGE_ALL;
      bindings.push_back(binding);
    }

    {
      std::unordered_map<uint32_t, size_t> binding_index_map;
      std::vector<VkDescriptorSetLayoutBinding> deduped;
      for (const auto &binding : bindings) {
        auto [it, inserted] = binding_index_map.try_emplace(
            binding.binding, deduped.size()
        );
        if (inserted) {
          deduped.push_back(binding);
        } else {
          deduped[it->second].stageFlags |= binding.stageFlags;
        }
      }
      bindings = std::move(deduped);
    }

    VkDescriptorSetLayoutCreateInfo create_info{};
    create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create_info.bindingCount = static_cast<uint32_t>(bindings.size());
    create_info.pBindings = bindings.data();

    VkResult result = vkCreateDescriptorSetLayout(
        m_device.logical_device(), &create_info, nullptr,
        &m_descriptor_set_layouts[set]);
    ASTRA_ENSURE(result != VK_SUCCESS,
                 "[Vulkan] Failed to create descriptor set layout");
  }
}

void VulkanShaderProgram::create_pipeline_layout() {
  VkPipelineLayoutCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  create_info.setLayoutCount =
      static_cast<uint32_t>(m_descriptor_set_layouts.size());
  create_info.pSetLayouts = m_descriptor_set_layouts.data();
  create_info.pushConstantRangeCount = 0;
  create_info.pPushConstantRanges = nullptr;

  VkResult result = vkCreatePipelineLayout(
      m_device.logical_device(), &create_info, nullptr, &m_pipeline_layout);
  ASTRA_ENSURE(result != VK_SUCCESS,
               "[Vulkan] Failed to create pipeline layout");
}

} // namespace astralix
