#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace astralix {

class VulkanDevice;

class VulkanSwapchain {
public:
  VulkanSwapchain(const VulkanDevice &device, VkSurfaceKHR surface,
                  uint32_t width, uint32_t height);
  ~VulkanSwapchain();

  VulkanSwapchain(const VulkanSwapchain &) = delete;
  VulkanSwapchain &operator=(const VulkanSwapchain &) = delete;

  void recreate(uint32_t width, uint32_t height);

  VkSwapchainKHR handle() const noexcept { return m_swapchain; }
  VkFormat image_format() const noexcept { return m_format.format; }
  VkExtent2D extent() const noexcept { return m_extent; }
  uint32_t image_count() const noexcept { return static_cast<uint32_t>(m_images.size()); }
  VkImage image(uint32_t index) const { return m_images[index]; }
  VkImageView image_view(uint32_t index) const { return m_image_views[index]; }

private:
  void create(uint32_t width, uint32_t height);
  void destroy_swapchain_resources();

  VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR> &formats) const;
  VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR> &modes) const;
  VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR &capabilities,
                           uint32_t width, uint32_t height) const;

  const VulkanDevice &m_device;
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
  VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
  VkSurfaceFormatKHR m_format{};
  VkExtent2D m_extent{};
  std::vector<VkImage> m_images;
  std::vector<VkImageView> m_image_views;
};

} // namespace astralix
