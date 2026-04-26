#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vulkan/vulkan.h>

namespace astralix {

class VulkanDevice;

class VulkanImage {
public:
  struct CreateInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t array_layers = 1;
    uint32_t mip_levels = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    VkImageCreateFlags flags = 0;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
  };

  VulkanImage(const VulkanDevice &device, const CreateInfo &info);
  ~VulkanImage();

  VulkanImage(const VulkanImage &) = delete;
  VulkanImage &operator=(const VulkanImage &) = delete;
  VulkanImage(VulkanImage &&other) noexcept;
  VulkanImage &operator=(VulkanImage &&other) noexcept;

  VkImage handle() const noexcept { return m_image; }
  VkImageView view() const noexcept { return m_view; }
  VkFormat format() const noexcept { return m_format; }
  uint32_t width() const noexcept { return m_width; }
  uint32_t height() const noexcept { return m_height; }
  uint32_t array_layers() const noexcept { return m_array_layers; }
  uint32_t mip_levels() const noexcept { return m_mip_levels; }
  VkImageAspectFlags aspect() const noexcept { return m_aspect; }
  VkSampleCountFlagBits samples() const noexcept { return m_samples; }
  VkImageLayout current_layout() const noexcept { return m_current_layout; }

  void set_current_layout(VkImageLayout layout) { m_current_layout = layout; }
  VkImageView view_for_subresource(
      VkImageAspectFlags aspect,
      uint32_t base_mip_level = 0,
      uint32_t level_count = 1,
      uint32_t base_array_layer = 0,
      uint32_t layer_count = 1,
      std::optional<VkImageViewType> view_type = std::nullopt
  );

  void transition(VkCommandBuffer command_buffer, VkImageLayout new_layout);

private:
  struct ViewKey {
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    uint32_t base_mip_level = 0;
    uint32_t level_count = 1;
    uint32_t base_array_layer = 0;
    uint32_t layer_count = 1;
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;

    bool operator==(const ViewKey &other) const noexcept {
      return aspect == other.aspect &&
             base_mip_level == other.base_mip_level &&
             level_count == other.level_count &&
             base_array_layer == other.base_array_layer &&
             layer_count == other.layer_count &&
             view_type == other.view_type;
    }
  };

  struct ViewKeyHash {
    size_t operator()(const ViewKey &key) const noexcept;
  };

  void destroy();

  const VulkanDevice *m_device = nullptr;
  VkImage m_image = VK_NULL_HANDLE;
  VkImageView m_view = VK_NULL_HANDLE;
  VkDeviceMemory m_memory = VK_NULL_HANDLE;
  VkFormat m_format = VK_FORMAT_UNDEFINED;
  VkImageAspectFlags m_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;
  VkImageLayout m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageViewType m_view_type = VK_IMAGE_VIEW_TYPE_2D;
  uint32_t m_width = 0;
  uint32_t m_height = 0;
  uint32_t m_array_layers = 1;
  uint32_t m_mip_levels = 1;
  std::unordered_map<ViewKey, VkImageView, ViewKeyHash> m_view_cache;
};

} // namespace astralix
