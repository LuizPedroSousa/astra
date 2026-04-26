#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace astralix {

class VulkanDevice;

class VulkanShaderModule {
public:
  VulkanShaderModule(const VulkanDevice &device,
                     const std::vector<uint32_t> &spirv,
                     VkShaderStageFlagBits stage);
  ~VulkanShaderModule();

  VulkanShaderModule(const VulkanShaderModule &) = delete;
  VulkanShaderModule &operator=(const VulkanShaderModule &) = delete;

  VkShaderModule handle() const noexcept { return m_module; }
  VkShaderStageFlagBits stage() const noexcept { return m_stage; }

private:
  const VulkanDevice &m_device;
  VkShaderModule m_module = VK_NULL_HANDLE;
  VkShaderStageFlagBits m_stage;
};

} // namespace astralix
