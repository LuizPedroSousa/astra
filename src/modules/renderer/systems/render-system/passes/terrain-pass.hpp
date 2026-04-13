#pragma once

#include "render-pass.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"

namespace astralix {

class TerrainRenderPass : public FramePass {
public:
  TerrainRenderPass() = default;
  ~TerrainRenderPass() override = default;

  void setup(PassSetupContext &ctx) override;
  void record(PassRecordContext &ctx, PassRecorder &recorder) override;
  std::string name() const override { return "TerrainRenderPass"; }

private:
  Ref<Shader> m_terrain_shader = nullptr;
};

} // namespace astralix
