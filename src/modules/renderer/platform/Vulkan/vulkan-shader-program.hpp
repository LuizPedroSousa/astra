#pragma once

#include "shader-lang/pipeline-layout.hpp"
#include <vulkan/vulkan.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace astralix {

class VulkanDevice;
class VulkanShaderModule;

class VulkanShaderProgram {
public:
  VulkanShaderProgram(
      const VulkanDevice &device,
      std::map<StageKind, std::vector<uint32_t>> spirv_stages,
      ShaderPipelineLayout layout);
  ~VulkanShaderProgram();

  VulkanShaderProgram(const VulkanShaderProgram &) = delete;
  VulkanShaderProgram &operator=(const VulkanShaderProgram &) = delete;

  uint32_t stage_count() const noexcept;
  std::vector<VkPipelineShaderStageCreateInfo> stage_create_infos() const;

  VkPipelineLayout pipeline_layout() const noexcept {
    return m_pipeline_layout;
  }

  VkDescriptorSetLayout descriptor_set_layout(uint32_t set) const;

  const ShaderValueBlockDesc *
  value_block(const std::string &logical_name) const;

  const ShaderResourceBindingDesc *
  resource_binding(uint64_t binding_id) const;

  const VertexInputLayoutDesc &vertex_input() const noexcept {
    return m_layout.vertex_input;
  }

  const ShaderPipelineLayout &layout() const noexcept { return m_layout; }

private:
  void create_descriptor_set_layouts();
  void create_pipeline_layout();

  const VulkanDevice &m_device;
  ShaderPipelineLayout m_layout;
  std::map<StageKind, std::unique_ptr<VulkanShaderModule>> m_modules;
  std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
  VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
};

} // namespace astralix
