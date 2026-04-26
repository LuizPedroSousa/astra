#pragma once

#include "vulkan-frame-context.hpp"

#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <vector>

namespace astralix {

class VulkanDevice;

class VulkanDescriptorAllocator {
public:
  explicit VulkanDescriptorAllocator(const VulkanDevice &device);
  ~VulkanDescriptorAllocator();

  VulkanDescriptorAllocator(const VulkanDescriptorAllocator &) = delete;
  VulkanDescriptorAllocator &operator=(const VulkanDescriptorAllocator &) = delete;

  VkDescriptorSet allocate_set(VkDescriptorSetLayout layout);
  void reset_frame(uint32_t frame_index);

private:
  struct FramePools {
    std::vector<VkDescriptorPool> pools;
    uint32_t current_pool_index = 0;
  };

  VkDescriptorPool create_pool();

  const VulkanDevice &m_device;
  std::array<FramePools, MAX_FRAMES_IN_FLIGHT> m_frame_pools{};
  uint32_t m_current_frame_index = 0;

  static constexpr uint32_t k_max_sets_per_pool = 256;
  static constexpr uint32_t k_uniform_buffers_per_pool = 512;
  static constexpr uint32_t k_combined_image_samplers_per_pool = 256;
  static constexpr uint32_t k_storage_buffers_per_pool = 128;
};

} // namespace astralix
