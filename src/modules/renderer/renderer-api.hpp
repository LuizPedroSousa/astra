#pragma once

#include "assert.hpp"
#include "base.hpp"
#include "framebuffer.hpp"
#include "glm/glm.hpp"
#include "systems/render-system/frame-stats.hpp"
#include "vertex-array.hpp"

namespace astralix {

enum class RendererBackend { None = 0, OpenGL = 1, Vulkan = 2 };

enum class ClearBufferType : uint32_t {
  None = 0,
  Color = 1 << 0,
  Depth = 1 << 1,
  Stencil = 1 << 2,
  All = Color | Depth | Stencil
};

inline ClearBufferType operator|(ClearBufferType a, ClearBufferType b) {
  return static_cast<ClearBufferType>(static_cast<uint32_t>(a) |
                                      static_cast<uint32_t>(b));
}

inline ClearBufferType operator&(ClearBufferType a, ClearBufferType b) {
  return static_cast<ClearBufferType>(static_cast<uint32_t>(a) &
                                      static_cast<uint32_t>(b));
}

inline ClearBufferType operator^(ClearBufferType a, ClearBufferType b) {
  return static_cast<ClearBufferType>(static_cast<uint32_t>(a) ^
                                      static_cast<uint32_t>(b));
}

inline ClearBufferType operator~(ClearBufferType a) {
  return static_cast<ClearBufferType>(~static_cast<uint32_t>(a));
}

inline ClearBufferType &operator|=(ClearBufferType &a, ClearBufferType b) {
  return a = a | b;
}

inline ClearBufferType &operator&=(ClearBufferType &a, ClearBufferType b) {
  return a = a & b;
}

inline ClearBufferType &operator^=(ClearBufferType &a, ClearBufferType b) {
  return a = a ^ b;
}

class VertexArray;

class RendererAPI {
public:
  enum DrawPrimitive { POINTS = 0, LINES = 1, TRIANGLES = 2 };
  enum CullFaceMode { Front = 0, Left = 1, Right = 2, Back = 3 };
  enum DepthMode { Equal = 0, Less = 1, LessEqual = 2 };
  enum BlendFactor { Zero = 0, One = 1, SrcAlpha = 2, OneMinusSrcAlpha = 3 };

  virtual void init() = 0;
  virtual void set_viewport(uint32_t x, uint32_t y, uint32_t width,
                            uint32_t height) = 0;
  virtual void clear_color(glm::vec4 color = glm::vec4(0.0f, 0.0f, 0.0f,
                                                       1.0f)) = 0;
  virtual void clear_buffers(ClearBufferType type = ClearBufferType::All) = 0;
  virtual void disable_buffer_testing() = 0;
  virtual void enable_buffer_testing() = 0;
  virtual void enable_depth_test() = 0;
  virtual void disable_depth_test() = 0;
  virtual void enable_depth_write() = 0;
  virtual void disable_depth_write() = 0;
  virtual void enable_depth_bias() = 0;
  virtual void disable_depth_bias() = 0;
  virtual void set_depth_bias(float slope_factor,
                              float constant_factor) = 0;
  virtual void enable_blend() = 0;
  virtual void disable_blend() = 0;
  virtual void set_blend_func(BlendFactor src, BlendFactor dst) = 0;
  virtual void enable_scissor() = 0;
  virtual void disable_scissor() = 0;
  virtual void set_scissor_rect(uint32_t x, uint32_t y, uint32_t width,
                                uint32_t height) = 0;
  virtual void enable_cull() = 0;
  virtual void disable_cull() = 0;
  virtual void bind_texture_2d(uint32_t texture_id, uint32_t slot) = 0;
  virtual void bind_texture_cube(uint32_t texture_id, uint32_t slot) = 0;

  virtual void cull_face(CullFaceMode mode) = 0;
  virtual void depth(DepthMode mode) = 0;

  virtual void
  draw_indexed(const Ref<VertexArray> &vertex_array,
               DrawPrimitive primitive_type = DrawPrimitive::TRIANGLES,
               uint32_t index_count = -1) = 0;

  virtual void draw_instanced_indexed(DrawPrimitive primitive_type,
                                      uint32_t index_count,
                                      uint32_t instance_count) = 0;

  virtual void draw_lines(const Ref<VertexArray> &vertex_array,
                          uint32_t vertex_count) = 0;

  virtual void draw_triangles(const Ref<VertexArray> &vertex_array,
                              uint32_t vertex_count) = 0;

  void reset_frame_stats() { m_frame_stats = {}; }
  const FrameStats &frame_stats() const { return m_frame_stats; }

  virtual void begin_gpu_timer() {}
  virtual void end_gpu_timer() {}
  virtual float read_gpu_timer_ms() { return 0.0f; }
  virtual void query_gpu_memory(float &used_mb, float &total_mb) {
    used_mb = 0.0f;
    total_mb = 0.0f;
  }

  RendererBackend get_backend() { return m_backend; };

  static Scope<RendererAPI> create(const RendererBackend &p_backend);

protected:
  RendererBackend m_backend;
  FrameStats m_frame_stats;
};

template <typename T, typename O, typename... Args>
Ref<T> create_renderer_component_ref(RendererBackend backend,
                                     Args &&...params) {
  switch (backend) {
  case RendererBackend::OpenGL:
    return create_ref<O>(std::forward<Args>(params)...);

  case RendererBackend::Vulkan:
    ASTRA_EXCEPTION("Vulkan resource creation through legacy factories is not supported");

  default:
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
}

template <typename T, typename O, typename... Args>
Scope<T> create_renderer_component_scope(RendererBackend api,
                                         Args &&...params) {
  switch (api) {
  case RendererBackend::OpenGL:
    return create_scope<O>(std::forward<Args>(params)...);

  case RendererBackend::Vulkan:
    ASTRA_EXCEPTION("Vulkan resource creation through legacy factories is not supported");

  default:
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
}

} // namespace astralix
