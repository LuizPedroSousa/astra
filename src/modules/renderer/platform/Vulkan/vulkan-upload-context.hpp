#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace astralix {

class VulkanUploadArena;

class VulkanUploadContext {
public:
  void copy_buffer(VkCommandBuffer command_buffer,
                   VkBuffer source, VkDeviceSize source_offset,
                   VkBuffer destination, VkDeviceSize destination_offset,
                   VkDeviceSize size);

  void copy_buffer_to_image(VkCommandBuffer command_buffer,
                            VkBuffer source, VkDeviceSize source_offset,
                            VkImage destination, uint32_t width, uint32_t height);

  void transition_image_layout(VkCommandBuffer command_buffer,
                               VkImage image,
                               VkImageLayout old_layout,
                               VkImageLayout new_layout);
};

} // namespace astralix
