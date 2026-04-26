#include "vulkan-descriptor-allocator.hpp"
#include "vulkan-device.hpp"
#include "assert.hpp"

namespace astralix {

VulkanDescriptorAllocator::VulkanDescriptorAllocator(const VulkanDevice &device)
    : m_device(device) {
  for (auto &frame_pools : m_frame_pools) {
    frame_pools.pools.push_back(create_pool());
  }
}

VulkanDescriptorAllocator::~VulkanDescriptorAllocator() {
  VkDevice device = m_device.logical_device();
  for (const auto &frame_pools : m_frame_pools) {
    for (VkDescriptorPool pool : frame_pools.pools) {
      vkDestroyDescriptorPool(device, pool, nullptr);
    }
  }
}

VkDescriptorSet VulkanDescriptorAllocator::allocate_set(VkDescriptorSetLayout layout) {
  auto &frame_pools = m_frame_pools[m_current_frame_index];

  VkDescriptorSetAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate_info.descriptorPool = frame_pools.pools[frame_pools.current_pool_index];
  allocate_info.descriptorSetCount = 1;
  allocate_info.pSetLayouts = &layout;

  VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
  VkResult result = vkAllocateDescriptorSets(
      m_device.logical_device(), &allocate_info, &descriptor_set);

  if (result == VK_ERROR_OUT_OF_POOL_MEMORY ||
      result == VK_ERROR_FRAGMENTED_POOL) {
    frame_pools.current_pool_index++;
    if (frame_pools.current_pool_index >= frame_pools.pools.size()) {
      frame_pools.pools.push_back(create_pool());
    }

    allocate_info.descriptorPool =
        frame_pools.pools[frame_pools.current_pool_index];
    result = vkAllocateDescriptorSets(
        m_device.logical_device(), &allocate_info, &descriptor_set);
    ASTRA_ENSURE(result != VK_SUCCESS,
                 "[Vulkan] Failed to allocate descriptor set after pool growth");
  }

  return descriptor_set;
}

void VulkanDescriptorAllocator::reset_frame(uint32_t frame_index) {
  m_current_frame_index = frame_index % MAX_FRAMES_IN_FLIGHT;

  auto &frame_pools = m_frame_pools[m_current_frame_index];
  VkDevice device = m_device.logical_device();
  for (VkDescriptorPool pool : frame_pools.pools) {
    vkResetDescriptorPool(device, pool, 0);
  }
  frame_pools.current_pool_index = 0;
}

VkDescriptorPool VulkanDescriptorAllocator::create_pool() {
  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, k_uniform_buffers_per_pool},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, k_combined_image_samplers_per_pool},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, k_storage_buffers_per_pool},
  };

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = k_max_sets_per_pool;
  pool_info.poolSizeCount = 3;
  pool_info.pPoolSizes = pool_sizes;

  VkDescriptorPool pool = VK_NULL_HANDLE;
  VkResult result = vkCreateDescriptorPool(
      m_device.logical_device(), &pool_info, nullptr, &pool);
  ASTRA_ENSURE(result != VK_SUCCESS,
               "[Vulkan] Failed to create descriptor pool");

  return pool;
}

} // namespace astralix
