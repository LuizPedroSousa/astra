#include "vulkan-instance.hpp"
#include "assert.hpp"
#include "log.hpp"
#include "vulkan-debug-messenger.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstring>

namespace astralix {

static constexpr const char *VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";

VulkanInstance::VulkanInstance() {
#ifdef NDEBUG
  m_validation_enabled = false;
#else
  m_validation_enabled = check_validation_layer_support();
#endif

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Astralix";
  app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app_info.pEngineName = "Astralix Engine";
  app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  app_info.apiVersion = VK_API_VERSION_1_3;

  auto extensions = get_required_extensions();

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  create_info.ppEnabledExtensionNames = extensions.data();

  VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
  if (m_validation_enabled) {
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = &VALIDATION_LAYER;

    debug_create_info = VulkanDebugMessenger::populate_create_info();
    create_info.pNext = &debug_create_info;
  } else {
    create_info.enabledLayerCount = 0;
    create_info.pNext = nullptr;
  }

  VkResult result = vkCreateInstance(&create_info, nullptr, &m_instance);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create instance, VkResult: ", static_cast<int>(result));

  LOG_INFO("[Vulkan] Instance created, validation: {}", m_validation_enabled ? "enabled" : "disabled");
}

VulkanInstance::~VulkanInstance() {
  if (m_instance != VK_NULL_HANDLE) {
    vkDestroyInstance(m_instance, nullptr);
  }
}

bool VulkanInstance::check_validation_layer_support() const {
  uint32_t layer_count = 0;
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

  std::vector<VkLayerProperties> available_layers(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

  for (const auto &layer : available_layers) {
    if (std::strcmp(layer.layerName, VALIDATION_LAYER) == 0) {
      return true;
    }
  }

  LOG_WARN("[Vulkan] Validation layer not available");
  return false;
}

std::vector<const char *> VulkanInstance::get_required_extensions() const {
  uint32_t glfw_extension_count = 0;
  const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

  std::vector<const char *> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

  if (m_validation_enabled) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

} // namespace astralix
