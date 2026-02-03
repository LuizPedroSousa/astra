#include "vertex-buffer.hpp"
#include "platform/OpenGL/opengl-vertex-buffer.hpp"
#include "renderer-api.hpp"

namespace astralix {

Ref<VertexBuffer> VertexBuffer::create(RendererBackend backend, uint32_t size) {
  return create_renderer_component_ref<VertexBuffer, OpenGLVertexBuffer>(
      backend, size);
};

Ref<VertexBuffer> VertexBuffer::create(RendererBackend backend,
                                       const void *vertices, uint32_t size,
                                       DrawType draw_type) {
  return create_renderer_component_ref<VertexBuffer, OpenGLVertexBuffer>(
      backend, vertices, size, draw_type);
};

} // namespace astralix
