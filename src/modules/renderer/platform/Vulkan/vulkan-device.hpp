#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <optional>

namespace astralix {

struct QueueFamilyIndices {
  std::optional<uint32_t> graphics_family;
  std::optional<uint32_t> present_family;

  bool is_complete() const {
    return graphics_family.has_value() && present_family.has_value();
  }
};

class VulkanDevice {
public:
  VulkanDevice(VkInstance instance, VkSurfaceKHR surface);
  ~VulkanDevice();

  VulkanDevice(const VulkanDevice &) = delete;
  VulkanDevice &operator=(const VulkanDevice &) = delete;

  VkDevice logical_device() const noexcept { return m_device; }
  VkPhysicalDevice physical_device() const noexcept { return m_physical_device; }
  VkQueue graphics_queue() const noexcept { return m_graphics_queue; }
  VkQueue present_queue() const noexcept { return m_present_queue; }
  const QueueFamilyIndices &queue_families() const noexcept { return m_queue_families; }
  VkPhysicalDeviceProperties physical_device_properties() const noexcept { return m_properties; }

  uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const;

private:
  void pick_physical_device(VkInstance instance, VkSurfaceKHR surface);
  void create_logical_device();
  QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) const;
  bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface) const;

  VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
  VkQueue m_graphics_queue = VK_NULL_HANDLE;
  VkQueue m_present_queue = VK_NULL_HANDLE;
  QueueFamilyIndices m_queue_families;
  VkPhysicalDeviceProperties m_properties{};
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
};

} // namespace astralix
