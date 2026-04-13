#include "vulkan-image.hpp"
#include "assert.hpp"
#include "vulkan-device.hpp"

#include <cstddef>
#include <functional>

namespace astralix {

VulkanImage::VulkanImage(const VulkanDevice &device, const CreateInfo &info)
    : m_device(&device), m_format(info.format), m_aspect(info.aspect),
      m_samples(info.samples), m_width(info.width), m_height(info.height) {
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.flags = info.flags;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = info.format;
  image_info.extent = {info.width, info.height, 1};
  image_info.mipLevels = info.mip_levels == 0 ? 1u : info.mip_levels;
  image_info.arrayLayers = info.array_layers;
  image_info.samples = info.samples;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = info.usage;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkResult result = vkCreateImage(device.logical_device(), &image_info, nullptr, &m_image);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create image");

  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(device.logical_device(), m_image, &memory_requirements);

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = memory_requirements.size;
  alloc_info.memoryTypeIndex = device.find_memory_type(
      memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  result = vkAllocateMemory(device.logical_device(), &alloc_info, nullptr, &m_memory);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to allocate image memory");

  vkBindImageMemory(device.logical_device(), m_image, m_memory, 0);

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = m_image;
  view_info.viewType = info.view_type;
  view_info.format = info.format;
  view_info.subresourceRange.aspectMask = info.aspect;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount =
      info.mip_levels == 0 ? 1u : info.mip_levels;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = info.array_layers;

  result = vkCreateImageView(device.logical_device(), &view_info, nullptr, &m_view);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create image view");
  m_array_layers = info.array_layers;
  m_mip_levels = info.mip_levels == 0 ? 1u : info.mip_levels;
  m_view_type = info.view_type;
}

VulkanImage::~VulkanImage() {
  destroy();
}

VulkanImage::VulkanImage(VulkanImage &&other) noexcept
    : m_device(other.m_device), m_image(other.m_image), m_view(other.m_view),
      m_memory(other.m_memory), m_format(other.m_format),
      m_aspect(other.m_aspect), m_samples(other.m_samples),
      m_current_layout(other.m_current_layout),
      m_view_type(other.m_view_type), m_width(other.m_width),
      m_height(other.m_height), m_array_layers(other.m_array_layers),
      m_mip_levels(other.m_mip_levels),
      m_view_cache(std::move(other.m_view_cache)) {
  other.m_image = VK_NULL_HANDLE;
  other.m_view = VK_NULL_HANDLE;
  other.m_memory = VK_NULL_HANDLE;
  other.m_view_cache.clear();
}

VulkanImage &VulkanImage::operator=(VulkanImage &&other) noexcept {
  if (this != &other) {
    destroy();
    m_device = other.m_device;
    m_image = other.m_image;
    m_view = other.m_view;
    m_memory = other.m_memory;
    m_format = other.m_format;
    m_aspect = other.m_aspect;
    m_samples = other.m_samples;
    m_current_layout = other.m_current_layout;
    m_view_type = other.m_view_type;
    m_width = other.m_width;
    m_height = other.m_height;
    m_array_layers = other.m_array_layers;
    m_mip_levels = other.m_mip_levels;
    m_view_cache = std::move(other.m_view_cache);
    other.m_image = VK_NULL_HANDLE;
    other.m_view = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_view_cache.clear();
  }
  return *this;
}

size_t VulkanImage::ViewKeyHash::operator()(const ViewKey &key) const noexcept {
  size_t seed = 0;
  const auto mix = [&seed](size_t value) {
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  };

  mix(std::hash<uint32_t>{}(key.aspect));
  mix(std::hash<uint32_t>{}(key.base_mip_level));
  mix(std::hash<uint32_t>{}(key.level_count));
  mix(std::hash<uint32_t>{}(key.base_array_layer));
  mix(std::hash<uint32_t>{}(key.layer_count));
  mix(std::hash<uint32_t>{}(static_cast<uint32_t>(key.view_type)));
  return seed;
}

VkImageView VulkanImage::view_for_subresource(
    VkImageAspectFlags aspect,
    uint32_t base_mip_level,
    uint32_t level_count,
    uint32_t base_array_layer,
    uint32_t layer_count,
    std::optional<VkImageViewType> view_type
) {
  const uint32_t effective_level_count = level_count == 0 ? 1u : level_count;
  const uint32_t effective_layer_count = layer_count == 0 ? 1u : layer_count;

  VkImageViewType effective_view_type = m_view_type;
  if (view_type.has_value()) {
    effective_view_type = *view_type;
  } else if (m_view_type == VK_IMAGE_VIEW_TYPE_CUBE) {
    effective_view_type =
        base_array_layer == 0 && effective_layer_count == m_array_layers
            ? VK_IMAGE_VIEW_TYPE_CUBE
            : VK_IMAGE_VIEW_TYPE_2D;
  } else if (m_array_layers > 1) {
    effective_view_type =
        effective_layer_count > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                  : VK_IMAGE_VIEW_TYPE_2D;
  }

  if (aspect == m_aspect && base_mip_level == 0 &&
      effective_level_count == m_mip_levels && base_array_layer == 0 &&
      effective_layer_count == m_array_layers &&
      effective_view_type == m_view_type) {
    return m_view;
  }

  const ViewKey key{
      .aspect = aspect,
      .base_mip_level = base_mip_level,
      .level_count = effective_level_count,
      .base_array_layer = base_array_layer,
      .layer_count = effective_layer_count,
      .view_type = effective_view_type,
  };
  if (const auto it = m_view_cache.find(key); it != m_view_cache.end()) {
    return it->second;
  }

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = m_image;
  view_info.viewType = effective_view_type;
  view_info.format = m_format;
  view_info.subresourceRange.aspectMask = aspect;
  view_info.subresourceRange.baseMipLevel = base_mip_level;
  view_info.subresourceRange.levelCount = effective_level_count;
  view_info.subresourceRange.baseArrayLayer = base_array_layer;
  view_info.subresourceRange.layerCount = effective_layer_count;

  VkImageView view = VK_NULL_HANDLE;
  const VkResult result =
      vkCreateImageView(m_device->logical_device(), &view_info, nullptr, &view);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create image subresource view");

  m_view_cache.emplace(key, view);
  return view;
}

void VulkanImage::transition(VkCommandBuffer command_buffer, VkImageLayout new_layout) {
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.oldLayout = m_current_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_image;
  barrier.subresourceRange.aspectMask = m_aspect;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = m_mip_levels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = m_array_layers;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  VkDependencyInfo dependency_info{};
  dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency_info.imageMemoryBarrierCount = 1;
  dependency_info.pImageMemoryBarriers = &barrier;

  vkCmdPipelineBarrier2(command_buffer, &dependency_info);
  m_current_layout = new_layout;
}

void VulkanImage::destroy() {
  if (m_device == nullptr) {
    return;
  }
  for (const auto &[_, view] : m_view_cache) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(m_device->logical_device(), view, nullptr);
    }
  }
  m_view_cache.clear();
  if (m_view != VK_NULL_HANDLE) {
    vkDestroyImageView(m_device->logical_device(), m_view, nullptr);
  }
  if (m_image != VK_NULL_HANDLE) {
    vkDestroyImage(m_device->logical_device(), m_image, nullptr);
  }
  if (m_memory != VK_NULL_HANDLE) {
    vkFreeMemory(m_device->logical_device(), m_memory, nullptr);
  }
}

} // namespace astralix
