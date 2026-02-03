#include "render-target.hpp"
#include "base.hpp"
#include "guid.hpp"
#include "managers/window-manager.hpp"
#include "renderer-api.hpp"

namespace astralix {

RenderTarget::RenderTarget(Scope<RendererAPI> renderer_api,
                           Ref<Framebuffer> framebuffer, MSAA msaa,
                           WindowID window_id)
    : m_renderer_api(std::move(renderer_api)), m_framebuffer(framebuffer),
      m_msaa(msaa), m_window_id(window_id) {}

void RenderTarget::init() { m_renderer_api->init(); }

void RenderTarget::bind(bool to_default_fb) {
  if (to_default_fb) {
    m_framebuffer->bind();
  } else {
    m_framebuffer->bind(FramebufferBindType::Default, 0);
  }

  m_renderer_api->enable_buffer_testing();
  m_renderer_api->clear_color();
  m_renderer_api->clear_buffers();
}

void RenderTarget::unbind() { m_framebuffer->unbind(); }

Ref<RenderTarget> RenderTarget::create(RendererBackend api, MSAA msaa,
                                       WindowID window_id) {
  switch (api) {
  case RendererBackend::OpenGL: {
    auto renderer_api = RendererAPI::create(api);

    auto window = window_manager()->get_window_by_id(window_id);

    FramebufferSpecification framebuffer_spec;
    framebuffer_spec.attachments = {
        FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RGBA32F,
        FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth};

    framebuffer_spec.width = window->width();
    framebuffer_spec.height = window->height();

    auto framebuffer = Framebuffer::create(api, framebuffer_spec);

    return create_ref<RenderTarget>(std::move(renderer_api),
                                    std::move(framebuffer), msaa, window_id);
  }

  default: {
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
  }
}

} // namespace astralix
