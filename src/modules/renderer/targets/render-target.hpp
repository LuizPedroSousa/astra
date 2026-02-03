#pragma once
#include "base.hpp"
#include "framebuffer.hpp"
#include "guid.hpp"
#include "renderer-api.hpp"

namespace astralix {

class RenderTarget {
public:
  struct MSAA {
    int samples;
    bool is_enabled;
  };
  void bind(bool to_default_fb = false);
  void unbind();
  void init();

  RenderTarget(Scope<RendererAPI> renderer_api, Ref<Framebuffer> framebuffer,
               MSAA msaa, WindowID window_id);
  ~RenderTarget() = default;

  static Ref<RenderTarget> create(RendererBackend api, MSAA msaa,
                                  WindowID window_id);

  bool has_msaa_enabled() const noexcept { return m_msaa.is_enabled; }

  inline RendererAPI *renderer_api() const noexcept {
    return m_renderer_api.get();
  }

  inline Ref<Framebuffer> framebuffer() const noexcept { return m_framebuffer; }
  inline MSAA msaa() const noexcept { return m_msaa; }
  inline WindowID window_id() const noexcept { return m_window_id; }

private:
  Ref<Framebuffer> m_framebuffer;
  Scope<RendererAPI> m_renderer_api;
  MSAA m_msaa;
  WindowID m_window_id;
};

} // namespace astralix
