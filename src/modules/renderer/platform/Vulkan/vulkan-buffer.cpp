#include "vulkan-buffer.hpp"
#include "vulkan-device.hpp"
#include "assert.hpp"

#include <cstring>

namespace astralix {

VulkanBuffer::VulkanBuffer(const VulkanDevice &device, VkDeviceSize size,
                           VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
    : m_device(device), m_size(size) {
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkResult result = vkCreateBuffer(device.logical_device(), &buffer_info, nullptr, &m_buffer);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create buffer");

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(device.logical_device(), m_buffer, &memory_requirements);

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = memory_requirements.size;
  alloc_info.memoryTypeIndex = device.find_memory_type(
      memory_requirements.memoryTypeBits, properties);

  result = vkAllocateMemory(device.logical_device(), &alloc_info, nullptr, &m_memory);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to allocate buffer memory");

  vkBindBufferMemory(device.logical_device(), m_buffer, m_memory, 0);
}

VulkanBuffer::~VulkanBuffer() {
  unmap();
  if (m_buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(m_device.logical_device(), m_buffer, nullptr);
  }
  if (m_memory != VK_NULL_HANDLE) {
    vkFreeMemory(m_device.logical_device(), m_memory, nullptr);
  }
}

void VulkanBuffer::map() {
  if (m_mapped == nullptr) {
    vkMapMemory(m_device.logical_device(), m_memory, 0, m_size, 0, &m_mapped);
  }
}

void VulkanBuffer::unmap() {
  if (m_mapped != nullptr) {
    vkUnmapMemory(m_device.logical_device(), m_memory);
    m_mapped = nullptr;
  }
}

void VulkanBuffer::upload(const void *data, VkDeviceSize size, VkDeviceSize offset) {
  map();
  std::memcpy(static_cast<char *>(m_mapped) + offset, data, size);
}

} // namespace astralix
