#pragma once

#include "base.hpp"
#include "components/component.hpp"
#include "framebuffer.hpp"
#include "targets/render-target.hpp"

namespace astralix {

class PostProcessingComponent : public Component<PostProcessingComponent> {
public:
  PostProcessingComponent(COMPONENT_INIT_PARAMS);
  ~PostProcessingComponent() = default;

  void start(Ref<RenderTarget> render_target);
  void post_update(Ref<RenderTarget> render_target);

private:
  void resolve_screen_texture(Ref<RenderTarget> render_target);

  Ref<Framebuffer> m_multisampled_framebuffer;
};
} // namespace astralix
