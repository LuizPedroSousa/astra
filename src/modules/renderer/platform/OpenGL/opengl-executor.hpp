#pragma once

#include "renderer-api.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "targets/render-target.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <optional>
#include <unordered_map>
#include <vector>

namespace astralix {

class OpenGLExecutor {
public:
  explicit OpenGLExecutor(RenderTarget &render_target);
  ~OpenGLExecutor();

  void execute(const CompiledFrame &frame);
  std::optional<int>
  read_pixel(const CompiledFrame &frame, ImageHandle src, int x, int y) const;

  struct ResolvedImageResource {
    uint32_t texture_id = 0;
    uint32_t texture_target = 0;
    ImageExtent extent{};
    ImageFormat format = ImageFormat::Undefined;
    ImageAspect aspect = ImageAspect::Color0;
    bool default_target = false;

    [[nodiscard]] bool valid() const noexcept {
      return default_target || texture_id != 0;
    }
  };

  struct CachedGraphImage {
    uint32_t texture_id = 0;
    uint32_t texture_target = 0;
    ImageDesc desc_snapshot{};
  };

private:

  struct FramebufferBindingState {
    uint32_t framebuffer_id = 0;
    bool default_target = false;
  };

  struct OpenGLStateCache {
    bool draw_binding_valid = false;
    bool read_binding_valid = false;
    FramebufferBindingState draw_binding{};
    FramebufferBindingState read_binding{};
    const Shader *shader = nullptr;
    Ref<VertexArray> vertex_array = nullptr;

    bool depth_test = false;
    bool depth_test_valid = false;
    bool depth_write = false;
    bool depth_write_valid = false;
    bool depth_bias = false;
    bool depth_bias_valid = false;
    float depth_bias_slope = 0.0f;
    float depth_bias_constant = 0.0f;
    bool depth_bias_value_valid = false;
    bool blend = false;
    bool blend_valid = false;
    bool cull = false;
    bool cull_valid = false;
    RendererAPI::DepthMode depth_mode = RendererAPI::DepthMode::Less;
    bool depth_mode_valid = false;
    RendererAPI::BlendFactor blend_src = RendererAPI::BlendFactor::One;
    RendererAPI::BlendFactor blend_dst = RendererAPI::BlendFactor::Zero;
    bool blend_func_valid = false;
    RendererAPI::CullFaceMode cull_face = RendererAPI::CullFaceMode::Back;
    bool cull_face_valid = false;
    std::vector<uint32_t> texture2d_slots;
    std::vector<uint32_t> texture_cube_slots;
    uint32_t next_texture_unit = 0;
    std::unordered_map<uint64_t, uint32_t> texture_unit_by_binding_id;
    bool scissor_enabled = false;
    bool scissor_valid = false;

    void reset();
  };

  void execute_pass(const CompiledPass &pass);
  void apply_barriers(const CompiledPass &pass);
  void blit_present_edges(const CompiledFrame &frame);

  void dispatch(const BeginRenderingCmd &cmd);
  void dispatch(const EndRenderingCmd &cmd);
  void dispatch(const BindPipelineCmd &cmd);
  void dispatch(const BindComputePipelineCmd &cmd);
  void dispatch(const BindBindingsCmd &cmd);
  void dispatch(const BindVertexBufferCmd &cmd);
  void dispatch(const BindIndexBufferCmd &cmd);
  void dispatch(const DrawIndexedCmd &cmd);
  void dispatch(const DispatchComputeCmd &cmd);
  void dispatch(const MemoryBarrierCmd &cmd);
  void dispatch(const CopyImageCmd &cmd);
  void dispatch(const ResolveImageCmd &cmd);
  void dispatch(const ReadbackImageCmd &cmd) const;
  void dispatch(const SetScissorCmd &cmd);
  void dispatch(const DrawVerticesCmd &cmd);
  void dispatch(const SetViewportCmd &cmd);

  const CompiledImage &require_image(ImageHandle handle) const;
  const CompiledPipeline &require_pipeline(RenderPipelineHandle handle) const;
  const CompiledBindingGroup &
  require_binding_group(RenderBindingGroupHandle handle) const;
  const CompiledBuffer &require_buffer(BufferHandle handle) const;

  ResolvedImageResource resolve_image(ImageHandle handle) const;
  ResolvedImageResource resolve_graph_image(const CompiledImage &image) const;
  uint32_t acquire_framebuffer(
      const std::vector<ResolvedImageResource> &color_attachments,
      const std::optional<ResolvedImageResource> &depth_attachment
  ) const;
  void bind_draw_framebuffer(uint32_t framebuffer_id, bool default_target);
  void bind_read_framebuffer(uint32_t framebuffer_id, bool default_target) const;
  uint32_t resolve_texture_id(ImageViewRef view) const;
  ClearBufferType clear_mask_for(const RenderingInfo &info) const;
  RendererAPI::DepthMode map_compare_op(CompareOp op) const;
  RendererAPI::CullFaceMode map_cull_mode(CullMode mode) const;
  void apply_value_binding(const Shader &shader,
                           const CompiledValueBinding &binding) const;
  void invalidate_framebuffer_cache() const;
  void destroy_cached_framebuffers() const;

  void ensure_transient_buffer_uploaded(const CompiledBuffer &buffer);

  RendererAPI &m_api;
  RenderTarget &m_render_target;
  mutable OpenGLStateCache m_state;
  mutable const CompiledFrame *m_frame = nullptr;
  mutable const CompiledPipeline *m_bound_pipeline = nullptr;
  mutable std::unordered_map<const RenderGraphImageResource *, CachedGraphImage>
      m_graph_images;
  mutable std::unordered_map<std::string, uint32_t> m_framebuffer_cache;
  Ref<VertexArray> m_transient_vertex_array;
  Ref<VertexBuffer> m_transient_vertex_buffer;
  size_t m_transient_capacity = 0;
  std::optional<BufferLayout> m_transient_layout;
  ImageExtent m_active_render_extent{};
};

} // namespace astralix
