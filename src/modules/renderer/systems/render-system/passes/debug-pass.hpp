#pragma once

#include "framebuffer.hpp"
#include "managers/resource-manager.hpp"
#include "render-pass.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-frame.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class DebugGBufferPass : public FramePass {
public:
  explicit DebugGBufferPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~DebugGBufferPass() override = default;

  void setup(PassSetupContext &ctx) override;
  void record(PassRecordContext &ctx, PassRecorder &recorder) override;

  std::string name() const override { return "DebugGBufferPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
  bool m_active = false;
  int m_attachment_index = 0;
};

class DebugOverlayPass : public FramePass {
public:
  explicit DebugOverlayPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~DebugOverlayPass() override = default;

  void setup(PassSetupContext &ctx) override;
  void record(PassRecordContext &ctx, PassRecorder &recorder) override;

  std::string name() const override { return "DebugOverlayPass"; }

private:
  Ref<Shader> m_depth_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
  bool m_depth_active = false;
  bool m_depth_fullscreen = false;
};

} // namespace astralix
