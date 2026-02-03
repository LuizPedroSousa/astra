#include "vertex-array.hpp"
#include "platform/OpenGL/opengl-vertex-array.hpp"
#include "renderer-api.hpp"

namespace astralix {
Ref<VertexArray> VertexArray::create(RendererBackend backend) {
  return create_renderer_component_ref<VertexArray, OpenGLVertexArray>(backend);
}
} // namespace astralix
