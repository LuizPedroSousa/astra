#pragma once

#include "framebuffer.hpp"
#include "glm/vec2.hpp"
#include "render-pass.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"

namespace astralix::rendering {

struct EntityPickReadbackRequest {
  bool armed = false;
  glm::ivec2 pixel{};
  int *out_value = nullptr;
  bool *out_ready = nullptr;
};

} // namespace astralix::rendering

namespace astralix {

class EntityPickReadbackPass : public FramePass {
public:
  explicit EntityPickReadbackPass(
      rendering::EntityPickReadbackRequest *request = nullptr
  )
      : m_request(request) {}

  void setup(PassSetupContext &ctx) override { (void)ctx; }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    const auto *entity_pick_resource = ctx.find_graph_image("entity_pick");

    if (entity_pick_resource == nullptr || m_request == nullptr ||
        !m_request->armed || m_request->out_value == nullptr) {
      return;
    }

    const auto &spec = entity_pick_resource->get_graph_image()->desc;
    if (m_request->pixel.x < 0 || m_request->pixel.y < 0 ||
        m_request->pixel.x >= static_cast<int>(spec.width) ||
        m_request->pixel.y >= static_cast<int>(spec.height)) {
      return;
    }

    auto &frame = ctx.frame();
    const auto entity_pick = ctx.register_graph_image(
        "entity-pick.readback", *entity_pick_resource
    );
    recorder.readback_image(
        entity_pick, m_request->pixel.x, m_request->pixel.y,
        m_request->out_value, m_request->out_ready
    );
  }

  bool has_side_effects() const override { return true; }

  std::string name() const override { return "EntityPickReadbackPass"; }

private:
  rendering::EntityPickReadbackRequest *m_request = nullptr;
};

} // namespace astralix
