#pragma once

#include "project.hpp"
#include "systems/system.hpp"
#include "systems/render-system/passes/render-graph.hpp"

namespace astralix {
class RenderSystem : public System<RenderSystem> {
public:
  RenderSystem(RenderSystemConfig &config);
  ~RenderSystem();

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

private:
  Ref<RenderTarget> m_render_target;
  RenderSystemConfig m_config;
  Scope<RenderGraph> m_render_graph;
};

} // namespace astralix
