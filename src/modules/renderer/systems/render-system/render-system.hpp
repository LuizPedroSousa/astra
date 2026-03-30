#pragma once

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

  const std::vector<RenderImageExportBinding> &
  render_image_exports() const noexcept {
    return m_render_image_exports;
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
  uint32_t m_g_buffer_resource_index = 0;
  bool m_has_g_buffer = false;
};

} // namespace astralix
