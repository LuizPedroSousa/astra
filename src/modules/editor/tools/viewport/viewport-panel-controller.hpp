#pragma once

#include "editor-camera-navigation-store.hpp"
#include "editor-gizmo-store.hpp"
#include "immediate.hpp"
#include "panels/panel-controller.hpp"
#include <array>
#include <string>

namespace astralix::editor {

class ViewportPanelController final : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 360.0f,
      .height = 240.0f,
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;
  std::optional<uint64_t> render_version() const override;

private:
  struct RenderViewSlot {
    std::string label;
    RenderImageExportKey key;
    float preview_exposure = 1.0f;
  };

  static constexpr size_t kAttachmentViewCount = 14u;

  void set_mode(EditorGizmoMode mode);
  void toggle_attachments();
  void toggle_grid();
  void toggle_snap();
  void swap_attachment_into_main(size_t index);
  void sync_panel_rect();

  ui::im::Runtime *m_runtime = nullptr;
  ui::im::WidgetId m_viewport_image_widget = ui::im::k_invalid_widget_id;
  RenderViewSlot m_main_view{
      .label = "Scene",
      .key = make_render_image_export_key(RenderImageResource::SceneColor),
  };
  std::array<RenderViewSlot, kAttachmentViewCount> m_attachment_views{{
      {
          .label = "Shadow",
          .key = make_render_image_export_key(
              RenderImageResource::ShadowMap, RenderImageAspect::Depth
          ),
      },
      {
          .label = "Position",
          .key = make_g_buffer_export_key(GBufferAspect::Position),
      },
      {
          .label = "Normal",
          .key = make_g_buffer_export_key(GBufferAspect::Normal),
      },
      {
          .label = "Geo Normal",
          .key = make_g_buffer_export_key(GBufferAspect::GeometricNormal),
      },
      {
          .label = "Albedo",
          .key = make_g_buffer_export_key(GBufferAspect::Albedo),
      },
      {
          .label = "Emissive/Bloom",
          .key = make_g_buffer_export_key(GBufferAspect::Emissive),
      },
      {
          .label = "SSAO",
          .key = make_render_image_export_key(RenderImageResource::SSAO),
      },
      {
          .label = "SSAO Blur",
          .key = make_render_image_export_key(RenderImageResource::SSAOBlur),
      },
      {
          .label = "SSGI",
          .key = make_render_image_export_key(RenderImageResource::SSGI),
          .preview_exposure = 10.0f,
      },
      {
          .label = "SSGI Blur",
          .key = make_render_image_export_key(RenderImageResource::SSGIBlur),
          .preview_exposure = 10.0f,
      },
      {
          .label = "SSGI Temporal",
          .key = make_render_image_export_key(RenderImageResource::SSGITemporal),
          .preview_exposure = 10.0f,
      },
      {
          .label = "SSGI History",
          .key = make_render_image_export_key(RenderImageResource::SSGIHistory),
          .preview_exposure = 10.0f,
      },
      {
          .label = "Velocity",
          .key = make_render_image_export_key(RenderImageResource::Velocity),
          .preview_exposure = 50.0f,
      },
      {
          .label = "Bloom",
          .key = make_render_image_export_key(RenderImageResource::Bloom),
          .preview_exposure = 5.0f,
      },
  }};
  bool m_show_attachments = false;
  bool m_show_grid = true;
  bool m_snap_enabled = false;
  uint64_t m_last_camera_navigation_revision = 0u;
  uint64_t m_toolbar_version = 0u;
  uint64_t m_last_rendered_toolbar_version = 0u;
};

} // namespace astralix::editor
