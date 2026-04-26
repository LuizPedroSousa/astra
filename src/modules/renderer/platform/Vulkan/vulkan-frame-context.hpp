#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace astralix {

class VulkanDevice;
class VulkanSwapchain;

static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

struct VulkanFrameData {
  VkCommandPool command_pool = VK_NULL_HANDLE;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  VkFence submit_fence = VK_NULL_HANDLE;
  VkSemaphore image_available = VK_NULL_HANDLE;
};

class VulkanFrameContext {
public:
  VulkanFrameContext(const VulkanDevice &device, uint32_t swapchain_image_count);
  ~VulkanFrameContext();

  VulkanFrameContext(const VulkanFrameContext &) = delete;
  VulkanFrameContext &operator=(const VulkanFrameContext &) = delete;

  void recreate_semaphores(uint32_t swapchain_image_count);

  enum class AcquireResult {
    Success,
    NeedsRecreate
  };

  AcquireResult begin_frame(VulkanSwapchain &swapchain);
  void end_frame();

  VkCommandBuffer command_buffer() const noexcept {
    return m_frames[m_current_frame].command_buffer;
  }
  VkFence submit_fence() const noexcept {
    return m_frames[m_current_frame].submit_fence;
  }
  VkSemaphore image_available_semaphore() const noexcept {
    return m_frames[m_current_frame].image_available;
  }
  VkSemaphore render_finished_semaphore() const noexcept {
    return m_render_finished_semaphores[m_swapchain_image_index];
  }
  uint32_t current_frame_index() const noexcept { return m_current_frame; }
  uint32_t swapchain_image_index() const noexcept { return m_swapchain_image_index; }
  uint64_t frame_serial() const noexcept { return m_frame_serial; }

  void advance_frame();

private:
  void create_render_finished_semaphores(uint32_t count);
  void destroy_render_finished_semaphores();

  const VulkanDevice &m_device;
  std::array<VulkanFrameData, MAX_FRAMES_IN_FLIGHT> m_frames{};
  std::vector<VkSemaphore> m_render_finished_semaphores;
  uint32_t m_current_frame = 0;
  uint32_t m_swapchain_image_index = 0;
  uint64_t m_frame_serial = 0;
};

} // namespace astralix
