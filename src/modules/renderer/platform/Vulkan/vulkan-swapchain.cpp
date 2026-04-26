#include "vulkan-swapchain.hpp"
#include "vulkan-device.hpp"
#include "assert.hpp"
#include "log.hpp"

#include <algorithm>
#include <limits>

namespace astralix {

VulkanSwapchain::VulkanSwapchain(const VulkanDevice &device, VkSurfaceKHR surface,
                                 uint32_t width, uint32_t height)
    : m_device(device), m_surface(surface) {
  create(width, height);
}

VulkanSwapchain::~VulkanSwapchain() {
  destroy_swapchain_resources();
}

void VulkanSwapchain::recreate(uint32_t width, uint32_t height) {
  vkDeviceWaitIdle(m_device.logical_device());
  destroy_swapchain_resources();
  create(width, height);
}

void VulkanSwapchain::create(uint32_t width, uint32_t height) {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      m_device.physical_device(), m_surface, &capabilities);

  uint32_t format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(
      m_device.physical_device(), m_surface, &format_count, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(
      m_device.physical_device(), m_surface, &format_count, formats.data());

  uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      m_device.physical_device(), m_surface, &present_mode_count, nullptr);
  std::vector<VkPresentModeKHR> present_modes(present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      m_device.physical_device(), m_surface, &present_mode_count, present_modes.data());

  m_format = choose_surface_format(formats);
  VkPresentModeKHR present_mode = choose_present_mode(present_modes);
  m_extent = choose_extent(capabilities, width, height);

  uint32_t image_count = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
    image_count = capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = m_surface;
  create_info.minImageCount = image_count;
  create_info.imageFormat = m_format.format;
  create_info.imageColorSpace = m_format.colorSpace;
  create_info.imageExtent = m_extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  const auto &queue_families = m_device.queue_families();
  uint32_t family_indices[] = {
      queue_families.graphics_family.value(),
      queue_families.present_family.value(),
  };

  if (queue_families.graphics_family != queue_families.present_family) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = family_indices;
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  create_info.preTransform = capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = VK_NULL_HANDLE;

  VkResult result = vkCreateSwapchainKHR(
      m_device.logical_device(), &create_info, nullptr, &m_swapchain);
  ASTRA_ENSURE(result != VK_SUCCESS,
               "[Vulkan] Failed to create swapchain, VkResult: ",
               static_cast<int>(result));

  uint32_t actual_image_count = 0;
  vkGetSwapchainImagesKHR(m_device.logical_device(), m_swapchain, &actual_image_count, nullptr);
  m_images.resize(actual_image_count);
  vkGetSwapchainImagesKHR(m_device.logical_device(), m_swapchain, &actual_image_count, m_images.data());

  m_image_views.resize(m_images.size());
  for (size_t i = 0; i < m_images.size(); ++i) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = m_images[i];
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = m_format.format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    result = vkCreateImageView(m_device.logical_device(), &view_info, nullptr, &m_image_views[i]);
    ASTRA_ENSURE(result != VK_SUCCESS,
                 "[Vulkan] Failed to create swapchain image view, VkResult: ",
                 static_cast<int>(result));
  }

  LOG_INFO("[Vulkan] Swapchain created: {}x{}, format={}, present_mode={}, images={}",
           m_extent.width, m_extent.height,
           static_cast<int>(m_format.format),
           static_cast<int>(present_mode),
           m_images.size());
}

void VulkanSwapchain::destroy_swapchain_resources() {
  for (auto view : m_image_views) {
    vkDestroyImageView(m_device.logical_device(), view, nullptr);
  }
  m_image_views.clear();
  m_images.clear();

  if (m_swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(m_device.logical_device(), m_swapchain, nullptr);
    m_swapchain = VK_NULL_HANDLE;
  }
}

VkSurfaceFormatKHR VulkanSwapchain::choose_surface_format(
    const std::vector<VkSurfaceFormatKHR> &formats) const {
  for (const auto &format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }

  for (const auto &format : formats) {
    if (format.format == VK_FORMAT_R8G8B8A8_UNORM &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }

  for (const auto &format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }

  return formats[0];
}

VkPresentModeKHR VulkanSwapchain::choose_present_mode(
    const std::vector<VkPresentModeKHR> &modes) const {
  for (const auto &mode : modes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return mode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::choose_extent(
    const VkSurfaceCapabilitiesKHR &capabilities,
    uint32_t width, uint32_t height) const {
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  VkExtent2D actual_extent = {width, height};
  actual_extent.width = std::clamp(
      actual_extent.width,
      capabilities.minImageExtent.width,
      capabilities.maxImageExtent.width);
  actual_extent.height = std::clamp(
      actual_extent.height,
      capabilities.minImageExtent.height,
      capabilities.maxImageExtent.height);
  return actual_extent;
}

} // namespace astralix
