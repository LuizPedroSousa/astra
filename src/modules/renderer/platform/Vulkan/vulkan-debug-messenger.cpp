#include "vulkan-debug-messenger.hpp"
#include "assert.hpp"
#include "log.hpp"

namespace astralix {

static VkResult create_debug_utils_messenger(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *create_info,
    const VkAllocationCallbacks *allocator,
    VkDebugUtilsMessengerEXT *messenger) {
  auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
  if (function != nullptr) {
    return function(instance, create_info, allocator, messenger);
  }
  return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void destroy_debug_utils_messenger(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks *allocator) {
  auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
  if (function != nullptr) {
    function(instance, messenger, allocator);
  }
}

VkDebugUtilsMessengerCreateInfoEXT VulkanDebugMessenger::populate_create_info() {
  VkDebugUtilsMessengerCreateInfoEXT create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  create_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  create_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info.pfnUserCallback = debug_callback;
  create_info.pUserData = nullptr;
  return create_info;
}

VulkanDebugMessenger::VulkanDebugMessenger(VkInstance instance)
    : m_instance(instance) {
  auto create_info = populate_create_info();

  VkResult result = create_debug_utils_messenger(
      m_instance, &create_info, nullptr, &m_messenger);
  ASTRA_ENSURE(result != VK_SUCCESS,
               "[Vulkan] Failed to create debug messenger, VkResult: ",
               static_cast<int>(result));

  LOG_INFO("[Vulkan] Debug messenger created");
}

VulkanDebugMessenger::~VulkanDebugMessenger() {
  if (m_messenger != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
    destroy_debug_utils_messenger(m_instance, m_messenger, nullptr);
  }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugMessenger::debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data) {
  (void)type;
  (void)user_data;

  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    LOG_ERROR("[Vulkan Validation] {}", callback_data->pMessage);
  } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    LOG_WARN("[Vulkan Validation] {}", callback_data->pMessage);
  }

  return VK_FALSE;
}

} // namespace astralix
