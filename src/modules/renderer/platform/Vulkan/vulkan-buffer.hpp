#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace astralix {

class VulkanDevice;

class VulkanBuffer {
public:
  VulkanBuffer(const VulkanDevice &device, VkDeviceSize size,
               VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
  ~VulkanBuffer();

  VulkanBuffer(const VulkanBuffer &) = delete;
  VulkanBuffer &operator=(const VulkanBuffer &) = delete;

  VkBuffer handle() const noexcept { return m_buffer; }
  VkDeviceSize size() const noexcept { return m_size; }
  void *mapped() const noexcept { return m_mapped; }

  void map();
  void unmap();
  void upload(const void *data, VkDeviceSize size, VkDeviceSize offset = 0);

private:
  const VulkanDevice &m_device;
  VkBuffer m_buffer = VK_NULL_HANDLE;
  VkDeviceMemory m_memory = VK_NULL_HANDLE;
  VkDeviceSize m_size = 0;
  void *m_mapped = nullptr;
};

} // namespace astralix
