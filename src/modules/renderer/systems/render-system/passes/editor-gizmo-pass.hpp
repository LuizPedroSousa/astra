#pragma once

#include "framebuffer.hpp"
#include "render-pass.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "trace.hpp"

namespace astralix {

class EditorGizmoPass : public FramePass {
public:
  EditorGizmoPass() = default;
  ~EditorGizmoPass() override = default;

  void setup(PassSetupContext &ctx) override;
  void record(PassRecordContext &ctx, PassRecorder &recorder) override;

  std::string name() const override { return "EditorGizmoPass"; }

private:
  Ref<Shader> m_shader = nullptr;
};

} // namespace astralix
