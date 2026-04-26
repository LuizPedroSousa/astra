#include "vulkan-device.hpp"
#include "assert.hpp"
#include "log.hpp"

#include <set>
#include <vector>

namespace astralix {

static const std::vector<const char *> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
};

VulkanDevice::VulkanDevice(VkInstance instance, VkSurfaceKHR surface)
    : m_surface(surface) {
  pick_physical_device(instance, surface);
  create_logical_device();
}

VulkanDevice::~VulkanDevice() {
  if (m_device != VK_NULL_HANDLE) {
    vkDestroyDevice(m_device, nullptr);
  }
}

void VulkanDevice::pick_physical_device(VkInstance instance, VkSurfaceKHR surface) {
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
  ASTRA_ENSURE(device_count == 0, "[Vulkan] No GPU with Vulkan support found");

  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

  VkPhysicalDevice fallback = VK_NULL_HANDLE;
  for (const auto &device : devices) {
    if (!is_device_suitable(device, surface)) {
      continue;
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      m_physical_device = device;
      m_properties = properties;
      break;
    }

    if (fallback == VK_NULL_HANDLE) {
      fallback = device;
      m_properties = properties;
    }
  }

  if (m_physical_device == VK_NULL_HANDLE) {
    m_physical_device = fallback;
  }

  ASTRA_ENSURE(m_physical_device == VK_NULL_HANDLE, "[Vulkan] Failed to find a suitable GPU");

  vkGetPhysicalDeviceProperties(m_physical_device, &m_properties);
  m_queue_families = find_queue_families(m_physical_device, surface);

  LOG_INFO("[Vulkan] Selected GPU: {}", m_properties.deviceName);
  LOG_INFO("[Vulkan] Graphics queue family: {}", m_queue_families.graphics_family.value());
  LOG_INFO("[Vulkan] Present queue family: {}", m_queue_families.present_family.value());
}

void VulkanDevice::create_logical_device() {
  std::set<uint32_t> unique_queue_families = {
      m_queue_families.graphics_family.value(),
      m_queue_families.present_family.value(),
  };

  float queue_priority = 1.0f;
  std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
  for (uint32_t queue_family : unique_queue_families) {
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(queue_create_info);
  }

  VkPhysicalDeviceVulkan13Features vulkan_13_features{};
  vulkan_13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  vulkan_13_features.dynamicRendering = VK_TRUE;
  vulkan_13_features.synchronization2 = VK_TRUE;

  VkPhysicalDeviceFeatures2 device_features{};
  device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  device_features.pNext = &vulkan_13_features;

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pNext = &device_features;
  create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
  create_info.pQueueCreateInfos = queue_create_infos.data();
  create_info.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
  create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

  VkResult result = vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create logical device, VkResult: ", static_cast<int>(result));

  vkGetDeviceQueue(m_device, m_queue_families.graphics_family.value(), 0, &m_graphics_queue);
  vkGetDeviceQueue(m_device, m_queue_families.present_family.value(), 0, &m_present_queue);

  LOG_INFO("[Vulkan] Logical device created");
}

QueueFamilyIndices VulkanDevice::find_queue_families(
    VkPhysicalDevice device, VkSurfaceKHR surface
) const {
  QueueFamilyIndices indices;

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

  for (uint32_t i = 0; i < queue_family_count; ++i) {
    if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphics_family = i;
    }

    VkBool32 present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
    if (present_support) {
      indices.present_family = i;
    }

    if (indices.is_complete()) {
      break;
    }
  }

  return indices;
}

bool VulkanDevice::is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface) const {
  auto indices = find_queue_families(device, surface);
  if (!indices.is_complete()) {
    return false;
  }

  uint32_t extension_count = 0;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

  std::vector<VkExtensionProperties> available_extensions(extension_count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

  std::set<std::string> required(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());
  for (const auto &extension : available_extensions) {
    required.erase(extension.extensionName);
  }

  return required.empty();
}

uint32_t VulkanDevice::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
  VkPhysicalDeviceMemoryProperties memory_properties;
  vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memory_properties);

  for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
    if ((type_filter & (1 << i)) &&
        (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  ASTRA_EXCEPTION("[Vulkan] Failed to find suitable memory type");
}

} // namespace astralix
