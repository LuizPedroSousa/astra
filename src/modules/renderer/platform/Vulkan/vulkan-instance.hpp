#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace astralix {

class VulkanInstance {
public:
  VulkanInstance();
  ~VulkanInstance();

  VulkanInstance(const VulkanInstance &) = delete;
  VulkanInstance &operator=(const VulkanInstance &) = delete;

  VkInstance handle() const noexcept { return m_instance; }
  bool validation_enabled() const noexcept { return m_validation_enabled; }

private:
  bool check_validation_layer_support() const;
  std::vector<const char *> get_required_extensions() const;

  VkInstance m_instance = VK_NULL_HANDLE;
  bool m_validation_enabled = false;
};

} // namespace astralix
