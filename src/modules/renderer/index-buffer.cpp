#include "index-buffer.hpp"
#include "platform/OpenGL/opengl-index-buffer.hpp"
#include "renderer-api.hpp"

namespace astralix {

Ref<IndexBuffer> IndexBuffer::create(RendererBackend backend,
                                     u_int32_t *indices, u_int32_t count) {
  return create_renderer_component_ref<IndexBuffer, OpenGLIndexBuffer>(
      backend, indices, count);
}

} // namespace astralix
