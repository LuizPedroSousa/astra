#include "vertex-array.hpp"
#include "platform/OpenGL/opengl-vertex-array.hpp"
#include "renderer-api.hpp"
#include "virtual-vertex-array.hpp"

namespace astralix {
Ref<VertexArray> VertexArray::create(RendererBackend backend) {
  switch (backend) {
  case RendererBackend::OpenGL:
    return create_ref<OpenGLVertexArray>();
  case RendererBackend::Vulkan:
    return create_ref<VirtualVertexArray>();
  default:
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
}
} // namespace astralix
