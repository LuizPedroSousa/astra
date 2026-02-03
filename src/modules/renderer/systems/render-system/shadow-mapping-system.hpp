#pragma once

#include "entities/object.hpp"
#include "framebuffer.hpp"
#include "systems/system.hpp"
#include "targets/render-target.hpp"

namespace astralix {

class ShadowMappingSystem : public System<ShadowMappingSystem> {
public:
  ShadowMappingSystem(Ref<RenderTarget> render_target);
  ~ShadowMappingSystem();

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

  Ref<Framebuffer> framebufer() const noexcept { return m_framebuffer; }

  void bind_depth(Object *object);

private:
  Ref<Framebuffer> m_framebuffer;
  Ref<RenderTarget> m_render_target;
};

} // namespace astralix
