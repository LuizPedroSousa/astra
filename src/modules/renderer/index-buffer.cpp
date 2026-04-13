#include "index-buffer.hpp"
#include "platform/OpenGL/opengl-index-buffer.hpp"
#include "renderer-api.hpp"
#include "virtual-index-buffer.hpp"

namespace astralix {

Ref<IndexBuffer> IndexBuffer::create(RendererBackend backend,
                                     u_int32_t *indices, u_int32_t count) {
  switch (backend) {
  case RendererBackend::OpenGL:
    return create_ref<OpenGLIndexBuffer>(indices, count);
  case RendererBackend::Vulkan:
    return create_ref<VirtualIndexBuffer>(indices, count);
  default:
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
}

} // namespace astralix
