#include "post-processing-component.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/post-processing/serializers/post-processing-serializer.hpp"
#include "components/resource/resource-component.hpp"
#include "glad/glad.h"
#include "managers/window-manager.hpp"
#include "window.hpp"

namespace astralix {
PostProcessingComponent::PostProcessingComponent(COMPONENT_INIT_PARAMS)
    : COMPONENT_INIT(PostProcessingComponent, "post-processing", true,
                     create_ref<PostProcessingComponentSerializer>(this)) {}

void PostProcessingComponent::start(Ref<RenderTarget> render_target) {
  auto owner = get_owner();
  auto mesh = owner->get_or_add_component<MeshComponent>();
  auto resource = owner->get_component<ResourceComponent>();

  if (render_target->has_msaa_enabled()) {
    auto framebuffer = render_target->framebuffer();

    FramebufferSpecification framebuffer_spec =
        framebuffer->get_specification();

    framebuffer_spec.samples = render_target->msaa().samples;

    m_multisampled_framebuffer = Framebuffer::create(
        render_target->renderer_api()->get_backend(), framebuffer_spec);
  }

  resource->shader()->set_int("screen_texture", 4);
}

void PostProcessingComponent::post_update(Ref<RenderTarget> render_target) {
  auto owner = get_owner();
  auto resource = owner->get_component<ResourceComponent>();

  bool is_msaa_enabled = render_target->has_msaa_enabled();

  if (is_msaa_enabled) {
    resolve_screen_texture(render_target);
  }

  auto shader = resource->shader();

  u_int32_t screen_texture =
      is_msaa_enabled ? m_multisampled_framebuffer->get_color_attachment_id()
                      : render_target->framebuffer()->get_color_attachment_id();

  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, screen_texture);
}

void PostProcessingComponent::resolve_screen_texture(
    Ref<RenderTarget> render_target) {
  auto window =
      WindowManager::get()->get_window_by_id(render_target->window_id());

  int width = window->width();
  int height = window->height();

  Ref<Framebuffer> framebuffer = render_target->framebuffer();

  framebuffer->bind(FramebufferBindType::Read);

  m_multisampled_framebuffer->bind(FramebufferBindType::Draw);

  const FramebufferSpecification &spec = framebuffer->get_specification();
  framebuffer->blit(spec.width, spec.height);

  framebuffer->unbind();
}

} // namespace astralix
