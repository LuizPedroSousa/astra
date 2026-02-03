#include "renderer-api.hpp"
#include "assert.hpp"
#include "platform/OpenGL/opengl-renderer-api.hpp"

namespace astralix {

Scope<RendererAPI> RendererAPI::create(const RendererBackend &p_backend) {
  return create_renderer_component_scope<RendererAPI, OpenGLRendererAPI>(
      p_backend);
}

} // namespace astralix
