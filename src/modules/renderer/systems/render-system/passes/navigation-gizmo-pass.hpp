#pragma once

#include "framebuffer.hpp"
#include "render-pass.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"

namespace astralix {

class NavigationGizmoPass : public FramePass {
public:
  NavigationGizmoPass() = default;
  ~NavigationGizmoPass() override = default;

  void setup(PassSetupContext &ctx) override;
  void record(PassRecordContext &ctx, PassRecorder &recorder) override;

  std::string name() const override { return "NavigationGizmoPass"; }

private:
  Ref<Shader> m_shader = nullptr;
};

} // namespace astralix
