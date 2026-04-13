#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace astralix {

class VulkanSurface {
public:
  VulkanSurface(VkInstance instance, GLFWwindow *window);
  ~VulkanSurface();

  VulkanSurface(const VulkanSurface &) = delete;
  VulkanSurface &operator=(const VulkanSurface &) = delete;

  VkSurfaceKHR handle() const noexcept { return m_surface; }

private:
  VkInstance m_instance = VK_NULL_HANDLE;
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
};

} // namespace astralix
