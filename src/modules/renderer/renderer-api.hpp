#pragma once

#include "assert.hpp"
#include "base.hpp"
#include "framebuffer.hpp"
#include "glm/glm.hpp"
#include "vertex-array.hpp"

namespace astralix {

enum class RendererBackend { None = 0, OpenGL = 1 };

class VertexArray;

class RendererAPI {
public:
  enum DrawPrimitive { POINTS = 0, LINES = 1, TRIANGLES = 2 };
  enum CullFaceMode { Front = 0, Left = 1, Right = 2, Back = 3 };
  enum DepthMode { Equal = 0, Less = 1, LessEqual = 2 };

  virtual void init() = 0;
  virtual void set_viewport(uint32_t x, uint32_t y, uint32_t width,
                            uint32_t height) = 0;
  virtual void clear_color() = 0;
  virtual void clear_buffers() = 0;
  virtual void disable_buffer_testing() = 0;
  virtual void enable_buffer_testing() = 0;

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

  RendererBackend get_backend() { return m_backend; };

  void set_clear_color(const glm::vec4 &p_clear_color) {
    m_clear_color = p_clear_color;
  };

  static Scope<RendererAPI> create(const RendererBackend &p_backend);

protected:
  glm::vec4 m_clear_color = glm::vec4(0.5f, 0.5f, 1.0f, 0.0f);
  RendererBackend m_backend;
};

template <typename T, typename O, typename... Args>
Ref<T> create_renderer_component_ref(RendererBackend backend,
                                     Args &&...params) {
  switch (backend) {
  case RendererBackend::OpenGL:
    return create_ref<O>(std::forward<Args>(params)...);

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

  default:
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
}

} // namespace astralix
