#include "framebuffer.hpp"
#include "assert.h"
#include "engine.hpp"
#include "platform/OpenGL/opengl-framebuffer.hpp"
#include "renderer-api.hpp"
#include "virtual-framebuffer.hpp"

namespace astralix {

Ref<Framebuffer> Framebuffer::create(RendererBackend backend,
                                     const FramebufferSpecification &spec) {
  switch (backend) {
  case RendererBackend::OpenGL:
    return create_ref<OpenGLFramebuffer>(spec);
  case RendererBackend::Vulkan:
    return create_ref<VirtualFramebuffer>(spec);
  default:
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
};

} // namespace astralix
