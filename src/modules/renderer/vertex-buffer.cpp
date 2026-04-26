#include "vertex-buffer.hpp"
#include "platform/OpenGL/opengl-vertex-buffer.hpp"
#include "renderer-api.hpp"
#include "virtual-vertex-buffer.hpp"

namespace astralix {

Ref<VertexBuffer> VertexBuffer::create(RendererBackend backend, uint32_t size) {
  switch (backend) {
  case RendererBackend::OpenGL:
    return create_ref<OpenGLVertexBuffer>(size);
  case RendererBackend::Vulkan:
    return create_ref<VirtualVertexBuffer>(size);
  default:
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
}

Ref<VertexBuffer> VertexBuffer::create(RendererBackend backend,
                                       const void *vertices, uint32_t size,
                                       DrawType draw_type) {
  switch (backend) {
  case RendererBackend::OpenGL:
    return create_ref<OpenGLVertexBuffer>(vertices, size, draw_type);
  case RendererBackend::Vulkan:
    return create_ref<VirtualVertexBuffer>(vertices, size, draw_type);
  default:
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
}

} // namespace astralix
