#include "framebuffer.hpp"
#include "assert.h"
#include "engine.hpp"
#include "platform/OpenGL/opengl-framebuffer.hpp"
#include "renderer-api.hpp"

namespace astralix {

Ref<Framebuffer> Framebuffer::create(RendererBackend backend,
                                     const FramebufferSpecification &spec) {
  return create_renderer_component_ref<Framebuffer, OpenGLFramebuffer>(backend,
                                                                       spec);
};

} // namespace astralix
