#pragma once

#include "glm/glm.hpp"
#include "project.hpp"
#include "systems/render-system/passes/entity-pick-readback-pass.hpp"
#include "systems/render-system/passes/render-graph.hpp"
#include "systems/render-system/render-frame.hpp"
#include "systems/render-system/render-image-export.hpp"
#include "systems/system.hpp"
#include <deque>
#include <memory>
#include <optional>
#include <vector>

namespace astralix {

struct EntityPickResult {
  uint64_t frame_serial = 0;
  glm::ivec2 pixel{};
  std::optional<EntityID> entity_id;
};

class RenderSystem : public System<RenderSystem> {
public:
  RenderSystem(RenderSystemConfig &config);
  ~RenderSystem();

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

  std::optional<ResolvedRenderImage>
  resolve_render_image(RenderImageExportKey key) const;
  std::optional<glm::ivec2> entity_selection_extent() const;
  void request_entity_pick(glm::ivec2 pixel);
  std::optional<EntityPickResult> consume_latest_entity_pick();

  const std::vector<RenderImageExportBinding> &
  render_image_exports() const noexcept {
    return m_render_image_exports;
  }

  const FrameStats &current_frame_stats() const {
    static const FrameStats k_empty;
    if (m_render_graph == nullptr) {
      return k_empty;
    }
    return m_render_graph->latest_frame_stats();
  }

private:
  struct PendingEntityPickRequest {
    glm::ivec2 pixel{};
  };

  struct PendingEntityPickSubmission {
    uint64_t frame_serial = 0;
    glm::ivec2 pixel{};
    std::shared_ptr<const std::vector<EntityID>> pick_id_lut;
    int raw_value = 0;
    bool ready = false;
  };

  void rebuild_render_image_exports();
  void ensure_pass_dependency_descriptors();
  void reset_render_graph_state();
  void drain_completed_entity_picks();

  Ref<RenderTarget> m_render_target;
  RenderSystemConfig m_config;
  Scope<RenderGraph> m_render_graph;
  std::vector<RenderImageExportBinding> m_render_image_exports;
  uint32_t m_shadow_map_resource_index = 0;
  uint32_t m_scene_color_resource_index = 0;
  uint32_t m_scene_depth_resource_index = 0;
  uint32_t m_bloom_extract_resource_index = 0;
  uint32_t m_present_resource_index = 0;
  uint32_t m_ssao_resource_index = 0;
  uint32_t m_ssao_blur_resource_index = 0;
  uint32_t m_bloom_resource_index = 0;
  uint32_t m_g_position_resource_index = 0;
  uint32_t m_g_normal_resource_index = 0;
  uint32_t m_g_albedo_resource_index = 0;
  uint32_t m_g_emissive_resource_index = 0;
  uint32_t m_g_entity_id_resource_index = 0;
  uint32_t m_entity_pick_resource_index = 0;
  uint64_t m_render_frame_serial = 0;
  std::optional<PendingEntityPickRequest> m_pending_entity_pick_request;
  std::deque<PendingEntityPickSubmission> m_pending_entity_pick_submissions;
  std::optional<EntityPickResult> m_latest_entity_pick_result;
  rendering::EntityPickReadbackRequest m_entity_pick_readback_request;
  rendering::RenderRuntimeStore m_render_runtime_store;
};

} // namespace astralix
