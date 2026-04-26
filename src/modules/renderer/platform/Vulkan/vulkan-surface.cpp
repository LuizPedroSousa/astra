#include "vulkan-surface.hpp"
#include "assert.hpp"
#include "log.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace astralix {

VulkanSurface::VulkanSurface(VkInstance instance, GLFWwindow *window)
    : m_instance(instance) {
  VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &m_surface);
  ASTRA_ENSURE(result != VK_SUCCESS,
               "[Vulkan] Failed to create window surface, VkResult: ",
               static_cast<int>(result));

  LOG_INFO("[Vulkan] Window surface created");
}

VulkanSurface::~VulkanSurface() {
  if (m_surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
  }
}

} // namespace astralix
