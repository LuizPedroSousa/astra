#pragma once

#include <vulkan/vulkan.h>

namespace astralix {

class VulkanDebugMessenger {
public:
  VulkanDebugMessenger(VkInstance instance);
  ~VulkanDebugMessenger();

  VulkanDebugMessenger(const VulkanDebugMessenger &) = delete;
  VulkanDebugMessenger &operator=(const VulkanDebugMessenger &) = delete;

  static VkDebugUtilsMessengerCreateInfoEXT populate_create_info();

private:
  static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
      VkDebugUtilsMessageSeverityFlagBitsEXT severity,
      VkDebugUtilsMessageTypeFlagsEXT type,
      const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
      void *user_data);

  VkInstance m_instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT m_messenger = VK_NULL_HANDLE;
};

} // namespace astralix
