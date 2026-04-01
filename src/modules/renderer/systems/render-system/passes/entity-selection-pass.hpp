#pragma once

#include "framebuffer.hpp"
#include "guid.hpp"
#include "render-pass.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "targets/render-target.hpp"

#include <vector>

namespace astralix {

class EntitySelectionPass : public RenderPass {
public:
  explicit EntitySelectionPass(std::vector<EntityID> *pick_id_lut = nullptr)
      : m_pick_id_lut(pick_id_lut) {}
  ~EntitySelectionPass() override = default;

  void setup(
      Ref<RenderTarget> render_target,
      const std::vector<const RenderGraphResource *> &resources
  ) override;
  void begin(double dt) override;
  void execute(double dt) override;
  void end(double dt) override;
  void cleanup() override;

  std::string name() const override { return "EntitySelectionPass"; }
  bool has_side_effects() const override { return true; }

private:
  Framebuffer *m_scene_color = nullptr;
  Ref<Shader> m_shader = nullptr;
  std::vector<EntityID> *m_pick_id_lut = nullptr;
};

} // namespace astralix
