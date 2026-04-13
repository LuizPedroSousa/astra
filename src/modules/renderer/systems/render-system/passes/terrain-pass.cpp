#include "terrain-pass.hpp"
#include "framebuffer.hpp"
#include "log.hpp"
#include "systems/render-system/render-frame.hpp"
#include "trace.hpp"

namespace astralix {

void TerrainRenderPass::setup(PassSetupContext &ctx) {
  m_terrain_shader = ctx.require_shader("terrain_shader");

  if (m_terrain_shader == nullptr) {
    LOG_WARN("[TerrainRenderPass] Missing graph dependency: terrain_shader");
    set_enabled(false);
  }
}

void TerrainRenderPass::record(PassRecordContext &ctx, PassRecorder &recorder) {
  ASTRA_PROFILE_N("TerrainRenderPass::record");

  if (m_terrain_shader == nullptr) {
    return;
  }
}

} // namespace astralix
