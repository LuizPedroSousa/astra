#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace astralix {

class VulkanDevice;

class VulkanUploadArena {
public:
  VulkanUploadArena(const VulkanDevice &device, VkDeviceSize capacity);
  ~VulkanUploadArena();

  VulkanUploadArena(const VulkanUploadArena &) = delete;
  VulkanUploadArena &operator=(const VulkanUploadArena &) = delete;

  struct Allocation {
    VkBuffer buffer;
    VkDeviceSize offset;
    void *mapped;
  };

  Allocation allocate(VkDeviceSize size, VkDeviceSize alignment = 16);
  void reset();

  VkBuffer buffer() const noexcept { return m_buffer; }

private:
  const VulkanDevice &m_device;
  VkBuffer m_buffer = VK_NULL_HANDLE;
  VkDeviceMemory m_memory = VK_NULL_HANDLE;
  void *m_mapped = nullptr;
  VkDeviceSize m_capacity = 0;
  VkDeviceSize m_offset = 0;
};

} // namespace astralix
