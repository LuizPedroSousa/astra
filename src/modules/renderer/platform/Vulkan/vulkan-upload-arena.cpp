#include "vulkan-upload-arena.hpp"
#include "vulkan-device.hpp"
#include "assert.hpp"

namespace astralix {

VulkanUploadArena::VulkanUploadArena(const VulkanDevice &device, VkDeviceSize capacity)
    : m_device(device), m_capacity(capacity) {
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = capacity;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkResult result = vkCreateBuffer(device.logical_device(), &buffer_info, nullptr, &m_buffer);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create upload staging buffer");

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(device.logical_device(), m_buffer, &memory_requirements);

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = memory_requirements.size;
  alloc_info.memoryTypeIndex = device.find_memory_type(
      memory_requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  result = vkAllocateMemory(device.logical_device(), &alloc_info, nullptr, &m_memory);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to allocate upload staging memory");

  vkBindBufferMemory(device.logical_device(), m_buffer, m_memory, 0);
  vkMapMemory(device.logical_device(), m_memory, 0, capacity, 0, &m_mapped);
}

VulkanUploadArena::~VulkanUploadArena() {
  if (m_mapped != nullptr) {
    vkUnmapMemory(m_device.logical_device(), m_memory);
  }
  if (m_buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(m_device.logical_device(), m_buffer, nullptr);
  }
  if (m_memory != VK_NULL_HANDLE) {
    vkFreeMemory(m_device.logical_device(), m_memory, nullptr);
  }
}

VulkanUploadArena::Allocation VulkanUploadArena::allocate(VkDeviceSize size, VkDeviceSize alignment) {
  ASTRA_ENSURE(!can_allocate(size, alignment),
               "[Vulkan] Upload arena out of space (requested=",
               static_cast<unsigned long long>(size),
               ", alignment=",
               static_cast<unsigned long long>(alignment),
               ", used=",
               static_cast<unsigned long long>(m_offset),
               ", capacity=",
               static_cast<unsigned long long>(m_capacity),
               ")");

  VkDeviceSize aligned_offset = (m_offset + alignment - 1) & ~(alignment - 1);

  Allocation allocation{};
  allocation.buffer = m_buffer;
  allocation.offset = aligned_offset;
  allocation.mapped = static_cast<char *>(m_mapped) + aligned_offset;

  m_offset = aligned_offset + size;
  return allocation;
}

bool VulkanUploadArena::can_allocate(
    VkDeviceSize size, VkDeviceSize alignment
) const {
  VkDeviceSize aligned_offset = (m_offset + alignment - 1) & ~(alignment - 1);
  return aligned_offset + size <= m_capacity;
}

void VulkanUploadArena::reset() {
  m_offset = 0;
}

} // namespace astralix
