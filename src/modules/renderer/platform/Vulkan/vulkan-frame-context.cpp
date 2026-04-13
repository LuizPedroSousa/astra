#include "vulkan-frame-context.hpp"
#include "assert.hpp"
#include "log.hpp"
#include "vulkan-device.hpp"
#include "vulkan-swapchain.hpp"

#include <limits>

namespace astralix {

VulkanFrameContext::VulkanFrameContext(const VulkanDevice &device, uint32_t swapchain_image_count)
    : m_device(device) {
  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (auto &frame : m_frames) {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = m_device.queue_families().graphics_family.value();

    VkResult result = vkCreateCommandPool(
        m_device.logical_device(), &pool_info, nullptr, &frame.command_pool
    );
    ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create command pool");

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = frame.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    result = vkAllocateCommandBuffers(
        m_device.logical_device(), &alloc_info, &frame.command_buffer
    );
    ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to allocate command buffer");

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    result = vkCreateFence(
        m_device.logical_device(), &fence_info, nullptr, &frame.submit_fence
    );
    ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create fence");

    result = vkCreateSemaphore(
        m_device.logical_device(), &semaphore_info, nullptr, &frame.image_available
    );
    ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create image_available semaphore");
  }

  create_render_finished_semaphores(swapchain_image_count);

  LOG_INFO("[Vulkan] Frame context created with {} frames in flight", MAX_FRAMES_IN_FLIGHT);
}

VulkanFrameContext::~VulkanFrameContext() {
  vkDeviceWaitIdle(m_device.logical_device());

  destroy_render_finished_semaphores();

  for (auto &frame : m_frames) {
    vkDestroySemaphore(m_device.logical_device(), frame.image_available, nullptr);
    vkDestroyFence(m_device.logical_device(), frame.submit_fence, nullptr);
    vkDestroyCommandPool(m_device.logical_device(), frame.command_pool, nullptr);
  }
}

void VulkanFrameContext::create_render_finished_semaphores(uint32_t count) {
  m_render_finished_semaphores.resize(count);

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (uint32_t i = 0; i < count; ++i) {
    VkResult result = vkCreateSemaphore(
        m_device.logical_device(), &semaphore_info, nullptr, &m_render_finished_semaphores[i]
    );
    ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to create render_finished semaphore");
  }
}

void VulkanFrameContext::destroy_render_finished_semaphores() {
  for (auto semaphore : m_render_finished_semaphores) {
    vkDestroySemaphore(m_device.logical_device(), semaphore, nullptr);
  }
  m_render_finished_semaphores.clear();
}

void VulkanFrameContext::recreate_semaphores(uint32_t swapchain_image_count) {
  vkDeviceWaitIdle(m_device.logical_device());
  destroy_render_finished_semaphores();
  create_render_finished_semaphores(swapchain_image_count);
}

VulkanFrameContext::AcquireResult VulkanFrameContext::begin_frame(VulkanSwapchain &swapchain) {
  auto &frame = m_frames[m_current_frame];

  vkWaitForFences(m_device.logical_device(), 1, &frame.submit_fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

  VkResult result = vkAcquireNextImageKHR(
      m_device.logical_device(), swapchain.handle(), std::numeric_limits<uint64_t>::max(), frame.image_available, VK_NULL_HANDLE, &m_swapchain_image_index
  );

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    return AcquireResult::NeedsRecreate;
  }

  ASTRA_ENSURE(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR, "[Vulkan] Failed to acquire swapchain image, VkResult: ", static_cast<int>(result));

  vkResetFences(m_device.logical_device(), 1, &frame.submit_fence);
  vkResetCommandPool(m_device.logical_device(), frame.command_pool, 0);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  result = vkBeginCommandBuffer(frame.command_buffer, &begin_info);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to begin command buffer");

  ++m_frame_serial;
  return AcquireResult::Success;
}

void VulkanFrameContext::end_frame() {
  auto &frame = m_frames[m_current_frame];
  VkResult result = vkEndCommandBuffer(frame.command_buffer);
  ASTRA_ENSURE(result != VK_SUCCESS, "[Vulkan] Failed to end command buffer");
}

void VulkanFrameContext::advance_frame() {
  m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

} // namespace astralix
