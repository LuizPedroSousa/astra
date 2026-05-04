#pragma once

#include "assert.hpp"
#include "render-ir.hpp"
#include <utility>

namespace astralix {

class RenderApiAccessScope {
public:
  class Guard {
  public:
    Guard() { ++depth(); }
    ~Guard() { --depth(); }

    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;
  };

  static bool active() noexcept { return depth() > 0; }

private:
  static int &depth() noexcept {
    static thread_local int value = 0;
    return value;
  }
};

class PassRecorder {
public:
  void begin_rendering(const RenderingInfo &info) {
    ASTRA_ENSURE(
        m_rendering_active,
        "PassRecorder does not support nested begin_rendering() calls"
    );
    m_rendering_active = true;
    m_commands.emplace_back(BeginRenderingCmd{info});
  }

  void end_rendering() {
    ASTRA_ENSURE(
        !m_rendering_active,
        "PassRecorder end_rendering() called without begin_rendering()"
    );
    m_rendering_active = false;
    m_commands.emplace_back(EndRenderingCmd{});
  }

  void bind_pipeline(RenderPipelineHandle pipeline) {
    m_commands.emplace_back(BindPipelineCmd{pipeline});
  }

  void bind_compute_pipeline(RenderPipelineHandle pipeline) {
    m_commands.emplace_back(BindComputePipelineCmd{pipeline});
  }

  void bind_binding_group(RenderBindingGroupHandle binding_group) {
    m_commands.emplace_back(BindBindingsCmd{binding_group});
  }

  void bind_vertex_buffer(BufferHandle buffer, uint32_t slot = 0, uint32_t offset = 0) {
    m_commands.emplace_back(BindVertexBufferCmd{
        .buffer = buffer,
        .slot = slot,
        .offset = offset,
    });
  }

  void bind_index_buffer(BufferHandle buffer, IndexType index_type = IndexType::Uint32, uint32_t offset = 0) {
    m_commands.emplace_back(BindIndexBufferCmd{
        .buffer = buffer,
        .index_type = index_type,
        .offset = offset,
    });
  }

  void draw_indexed(const DrawIndexedArgs &args) {
    m_commands.emplace_back(DrawIndexedCmd{args});
  }

  void dispatch_compute(
      uint32_t group_count_x,
      uint32_t group_count_y = 1,
      uint32_t group_count_z = 1
  ) {
    m_commands.emplace_back(DispatchComputeCmd{
        .group_count_x = group_count_x,
        .group_count_y = group_count_y,
        .group_count_z = group_count_z,
    });
  }

  void memory_barrier(MemoryBarrierBit barriers) {
    m_commands.emplace_back(MemoryBarrierCmd{.barriers = barriers});
  }

  void copy_image(ImageHandle src, ImageHandle dst, const CopyRegion &region) {
    m_commands.emplace_back(CopyImageCmd{
        .src = src,
        .dst = dst,
        .region = region,
    });
  }

  void resolve_image(ImageHandle src, ImageHandle dst) {
    m_commands.emplace_back(ResolveImageCmd{
        .src = src,
        .dst = dst,
    });
  }

  void readback_image(
      ImageHandle src, int x, int y, int *out_value, bool *out_ready
  ) {
    m_commands.emplace_back(ReadbackImageCmd{
        .src = src,
        .x = x,
        .y = y,
        .out_value = out_value,
        .out_ready = out_ready,
    });
  }

  void set_scissor(bool enabled, uint32_t x = 0, uint32_t y = 0, uint32_t width = 0, uint32_t height = 0) {
    m_commands.emplace_back(SetScissorCmd{
        .enabled = enabled,
        .x = x,
        .y = y,
        .width = width,
        .height = height,
    });
  }

  void draw_vertices(uint32_t vertex_count, uint32_t first_vertex = 0) {
    m_commands.emplace_back(DrawVerticesCmd{
        .vertex_count = vertex_count,
        .first_vertex = first_vertex,
    });
  }

  void set_viewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    m_commands.emplace_back(SetViewportCmd{
        .x = x,
        .y = y,
        .width = width,
        .height = height,
    });
  }

  [[nodiscard]] const RenderCommandBuffer &commands() const noexcept {
    return m_commands;
  }

  [[nodiscard]] bool is_rendering_active() const noexcept {
    return m_rendering_active;
  }

  RenderCommandBuffer take_commands() {
    ASTRA_ENSURE(
        m_rendering_active,
        "PassRecorder finished with unbalanced begin_rendering()/end_rendering()"
    );
    return std::move(m_commands);
  }

private:
  RenderCommandBuffer m_commands;
  bool m_rendering_active = false;
};

} // namespace astralix
