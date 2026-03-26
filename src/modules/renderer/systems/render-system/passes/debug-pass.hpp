#pragma once

#include "managers/resource-manager.hpp"
#include "managers/window-manager.hpp"
#include "render-pass.hpp"
#include "resources/mesh.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "targets/render-target.hpp"

namespace astralix {

struct DebugDepthState {
  Mesh mesh = Mesh::quad(1.0f);
  Ref<Shader> shader = nullptr;
  bool active = false;
  bool fullscreen = false;
};

struct DebugNormalState {
  Ref<Shader> shader = nullptr;
  bool active = false;
};

struct DebugGBufferState {
  Mesh mesh = Mesh::quad(1.0f);
  Ref<Shader> shader = nullptr;
  bool active = false;
  int attachment_index = 0;
};

class DebugGBufferPass : public RenderPass {
public:
  DebugGBufferPass() = default;
  ~DebugGBufferPass() override = default;

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override;
  void begin(double dt) override;
  void execute(double dt) override;
  void end(double dt) override;
  void cleanup() override;

  std::string name() const override { return "DebugGBufferPass"; }

private:
  void draw_gbuffer();

  DebugGBufferState m_debug_gbuffer;
  Framebuffer *m_scene_color = nullptr;
  Framebuffer *m_g_buffer = nullptr;
};

class DebugOverlayPass : public RenderPass {
public:
  DebugOverlayPass() = default;
  ~DebugOverlayPass() override = default;

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override;
  void begin(double dt) override;
  void execute(double dt) override;
  void end(double dt) override;
  void cleanup() override;

  std::string name() const override { return "DebugOverlayPass"; }

private:
  void draw_depth_overlay();
  void draw_normal_overlay();

  DebugDepthState m_debug_depth;
  DebugNormalState m_debug_normal;
  Framebuffer *m_scene_color = nullptr;
  Framebuffer *m_shadow_map = nullptr;
};

} // namespace astralix
