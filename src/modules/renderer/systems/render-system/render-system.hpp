#pragma once

#include "glm/glm.hpp"
#include "project.hpp"
#include "systems/render-system/passes/render-graph.hpp"
#include "systems/render-system/render-image-export.hpp"
#include "systems/system.hpp"
#include <optional>
#include <vector>

namespace astralix {
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
  std::optional<EntityID> read_entity_id_at_pixel(int x, int y) const;

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
  void rebuild_render_image_exports();
  const RenderImageExportBinding *
  find_render_image_export(RenderImageExportKey key) const;

  std::optional<ResolvedRenderImage> resolve_direct_color_attachment(
      const RenderGraphResource &resource, uint32_t attachment_index
  ) const;
  std::optional<ResolvedRenderImage>
  resolve_direct_depth_attachment(const RenderGraphResource &resource) const;
  std::optional<ResolvedRenderImage> resolve_materialized_render_image(
      const RenderImageExportBinding &binding,
      const RenderGraphResource &resource
  ) const;

  Ref<RenderTarget> m_render_target;
  RenderSystemConfig m_config;
  Scope<RenderGraph> m_render_graph;
  std::vector<RenderImageExportBinding> m_render_image_exports;
  uint32_t m_shadow_map_resource_index = 0;
  uint32_t m_scene_color_resource_index = 0;
  uint32_t m_ssao_resource_index = 0;
  uint32_t m_ssao_blur_resource_index = 0;
  uint32_t m_bloom_resource_index = 0;
  uint32_t m_g_buffer_resource_index = 0;
  bool m_has_g_buffer = false;
  uint32_t m_entity_pick_resource_index = 0;
  int m_entity_pick_attachment = 0;
  std::vector<EntityID> m_entity_pick_ids;
};

} // namespace astralix
