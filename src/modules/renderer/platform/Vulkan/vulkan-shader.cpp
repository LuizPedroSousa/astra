#include "vulkan-shader.hpp"
#include "vulkan-device.hpp"
#include "assert.hpp"

namespace astralix {

VulkanShaderModule::VulkanShaderModule(const VulkanDevice &device,
                                       const std::vector<uint32_t> &spirv,
                                       VkShaderStageFlagBits stage)
    : m_device(device), m_stage(stage) {
  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = spirv.size() * sizeof(uint32_t);
  create_info.pCode = spirv.data();

  VkResult result = vkCreateShaderModule(
      device.logical_device(), &create_info, nullptr, &m_module);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create shader module");
}

VulkanShaderModule::~VulkanShaderModule() {
  if (m_module != VK_NULL_HANDLE) {
    vkDestroyShaderModule(m_device.logical_device(), m_module, nullptr);
  }
}

} // namespace astralix
