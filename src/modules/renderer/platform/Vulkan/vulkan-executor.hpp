#pragma once

#include "trace.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "vulkan-buffer.hpp"
#include "vulkan-debug-messenger.hpp"
#include "vulkan-descriptor-allocator.hpp"
#include "vulkan-device.hpp"
#include "vulkan-frame-context.hpp"
#include "vulkan-image.hpp"
#include "vulkan-instance.hpp"
#include "vulkan-pipeline-cache.hpp"
#include "vulkan-shader-program.hpp"
#include "vulkan-surface.hpp"
#include "vulkan-swapchain.hpp"
#include "vulkan-upload-arena.hpp"
#include "vulkan-upload-context.hpp"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct GLFWwindow;

namespace astralix {

class VulkanExecutor {
public:
  VulkanExecutor(GLFWwindow *window, uint32_t width, uint32_t height);
  ~VulkanExecutor();

  VulkanExecutor(const VulkanExecutor &) = delete;
  VulkanExecutor &operator=(const VulkanExecutor &) = delete;

  void execute(const CompiledFrame &frame);
  void handle_resize(uint32_t width, uint32_t height);

  std::optional<int> read_pixel(const CompiledFrame &frame, ImageHandle src, int x, int y) const;

  void register_shader_program(const std::string &descriptor_id, Scope<VulkanShaderProgram> program);
  VulkanShaderProgram *find_shader_program(const std::string &descriptor_id) const;

  const VulkanDevice &device() const noexcept { return *m_device; }

private:
  struct ResolvedImageResource {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    VkImageAspectFlags aspect = 0;
    uint32_t base_mip_level = 0;
    uint32_t level_count = 1;
    uint32_t base_array_layer = 0;
    uint32_t layer_count = 1;
    VulkanImage *owned_image = nullptr;
    uint32_t swapchain_image_index = std::numeric_limits<uint32_t>::max();

    [[nodiscard]] bool is_swapchain() const noexcept {
      return swapchain_image_index != std::numeric_limits<uint32_t>::max();
    }

    [[nodiscard]] bool valid() const noexcept {
      return image != VK_NULL_HANDLE && view != VK_NULL_HANDLE;
    }
  };

  struct PendingReadback {
    Scope<VulkanBuffer> buffer;
    int *out_value = nullptr;
    bool *out_ready = nullptr;
    VkFormat format = VK_FORMAT_UNDEFINED;
  };

  struct UploadedBufferBinding {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
  };

  struct DescriptorSetCacheKey {
    RenderBindingLayoutKey layout_key;
    RenderBindingReuseIdentity reuse_identity;
    uint64_t content_hash = 0;

    friend bool operator==(const DescriptorSetCacheKey &,
                           const DescriptorSetCacheKey &) = default;
  };

  struct DescriptorSetCacheKeyHash {
    size_t operator()(const DescriptorSetCacheKey &key) const noexcept;
  };

  VulkanShaderProgram *ensure_shader_program(const CompiledPipeline &pipeline);
  void recreate_swapchain();
  void submit_and_present();
  void ensure_default_samplers();
  void ensure_default_images_initialized(VkCommandBuffer command_buffer);
  VkSampler sampler_for_format(VkFormat format, VkImageAspectFlags aspect);
  VulkanUploadArena &current_upload_arena();
  void transition_swapchain_image(VkCommandBuffer command_buffer, VkImageLayout new_layout);
  void transition_image(VkCommandBuffer command_buffer, const ResolvedImageResource &image, VkImageLayout new_layout);
  VkImageLayout tracked_layout_for(const ResolvedImageResource &image) const;
  void collect_completed_readbacks(uint32_t frame_index);
  void blit_present_edges(VkCommandBuffer command_buffer, const CompiledFrame &frame);
  void clear_bound_pipeline_state();
  bool try_bind_current_pipeline(VkCommandBuffer command_buffer);

  ResolvedImageResource resolve_image_handle(ImageHandle handle);
  ResolvedImageResource resolve_image_view(
      const ImageViewRef &view,
      bool exact_subresource,
      std::optional<VkImageViewType> preferred_view_type = std::nullopt
  );
  ResolvedImageResource resolve_texture_2d(
      const CompiledImage &compiled_image, const Texture &texture
  );
  ResolvedImageResource resolve_texture_cube(
      const CompiledImage &compiled_image, const Texture &texture
  );
  ResolvedImageResource resolve_graph_image(const CompiledImage &compiled_image);
  ResolvedImageResource resolve_default_sampler_image(
      const ShaderResourceBindingDesc &resource
  ) const;
  bool image_requires_depth_layout(const ResolvedImageResource &image) const;

  void dispatch(VkCommandBuffer command_buffer, const BeginRenderingCmd &command);
  void dispatch(VkCommandBuffer command_buffer, const EndRenderingCmd &command);
  void dispatch(VkCommandBuffer command_buffer, const BindPipelineCmd &command);
  void dispatch(VkCommandBuffer command_buffer, const BindBindingsCmd &command);
  void dispatch(VkCommandBuffer command_buffer, const BindVertexBufferCmd &command);
  void dispatch(VkCommandBuffer command_buffer, const BindIndexBufferCmd &command);
  void dispatch(VkCommandBuffer command_buffer, const DrawIndexedCmd &command);
  void dispatch(VkCommandBuffer command_buffer, const DrawVerticesCmd &command);
  void dispatch(VkCommandBuffer command_buffer, const CopyImageCmd &command);
  void dispatch(VkCommandBuffer command_buffer, const ResolveImageCmd &command);
  void dispatch(VkCommandBuffer command_buffer, const ReadbackImageCmd &command);
  void dispatch(VkCommandBuffer command_buffer, const SetScissorCmd &command);
  bool should_cache_binding_group(const CompiledBindingGroup &binding_group) const;

  Scope<VulkanInstance> m_instance;
  Scope<VulkanDebugMessenger> m_debug_messenger;
  Scope<VulkanSurface> m_surface;
  Scope<VulkanDevice> m_device;
  Scope<VulkanSwapchain> m_swapchain;
  Scope<VulkanFrameContext> m_frame_context;
  Scope<VulkanDescriptorAllocator> m_descriptor_allocator;
  Scope<VulkanPipelineCache> m_pipeline_cache;
  std::array<Scope<VulkanUploadArena>, MAX_FRAMES_IN_FLIGHT>
      m_upload_arenas;
  VulkanUploadContext m_upload_context;
  VkSampler m_default_sampler = VK_NULL_HANDLE;
  VkSampler m_nearest_sampler = VK_NULL_HANDLE;
  VkSampler m_depth_sampler = VK_NULL_HANDLE;
  Scope<VulkanImage> m_default_image;
  Scope<VulkanImage> m_black_image;
  Scope<VulkanImage> m_flat_normal_image;

  std::unordered_map<std::string, Scope<VulkanShaderProgram>> m_shader_registry;
  std::unordered_map<const RenderGraphImageResource *, Scope<VulkanImage>>
      m_graph_images;
  std::unordered_map<const RenderGraphImageResource *, uint32_t>
      m_graph_image_generations;
  std::unordered_map<const Texture *, Scope<VulkanImage>> m_texture_images;
  std::vector<VkImageLayout> m_swapchain_image_layouts;
  std::vector<VkFormat> m_active_color_formats;
  VkFormat m_active_depth_format = VK_FORMAT_UNDEFINED;
  VkExtent2D m_active_render_extent{};
  AstraVkTraceContext m_tracy_graphics_ctx = nullptr;
  std::array<std::vector<PendingReadback>, MAX_FRAMES_IN_FLIGHT>
      m_pending_readbacks;
  std::unordered_map<uint32_t, UploadedBufferBinding> m_uploaded_vertex_buffers;
  std::unordered_map<uint32_t, UploadedBufferBinding> m_uploaded_index_buffers;
  std::array<std::unordered_map<DescriptorSetCacheKey, VkDescriptorSet, DescriptorSetCacheKeyHash>, MAX_FRAMES_IN_FLIGHT>
      m_descriptor_set_cache;

  const CompiledFrame *m_frame = nullptr;
  const CompiledPipeline *m_current_pipeline = nullptr;
  VkPipeline m_bound_pipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_bound_pipeline_layout = VK_NULL_HANDLE;
  const VulkanShaderProgram *m_bound_program = nullptr;
  BufferLayout m_bound_vertex_layout;
  bool m_has_bound_vertex_layout = false;

  bool m_needs_recreate = false;
  uint32_t m_width = 0;
  uint32_t m_height = 0;
};

} // namespace astralix
