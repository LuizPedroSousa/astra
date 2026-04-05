#pragma once

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
  };

  static constexpr size_t kAttachmentViewCount = 8u;

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
      .key = RenderImageExportKey{
          .resource = RenderImageResource::SceneColor,
          .aspect = RenderImageAspect::Color0,
      },
  };
  std::array<RenderViewSlot, kAttachmentViewCount> m_attachment_views{{
      {
          .label = "Shadow",
          .key = RenderImageExportKey{
              .resource = RenderImageResource::ShadowMap,
              .aspect = RenderImageAspect::Depth,
          },
      },
      {
          .label = "Position",
          .key = RenderImageExportKey{
              .resource = RenderImageResource::GBuffer,
              .aspect = RenderImageAspect::Color0,
          },
      },
      {
          .label = "Normal",
          .key = RenderImageExportKey{
              .resource = RenderImageResource::GBuffer,
              .aspect = RenderImageAspect::Color1,
          },
      },
      {
          .label = "Albedo",
          .key = RenderImageExportKey{
              .resource = RenderImageResource::GBuffer,
              .aspect = RenderImageAspect::Color2,
          },
      },
      {
          .label = "Emissive/Bloom",
          .key = RenderImageExportKey{
              .resource = RenderImageResource::GBuffer,
              .aspect = RenderImageAspect::Color3,
          },
      },
      {
          .label = "SSAO",
          .key = RenderImageExportKey{
              .resource = RenderImageResource::SSAO,
              .aspect = RenderImageAspect::Color0,
          },
      },
      {
          .label = "SSAO Blur",
          .key = RenderImageExportKey{
              .resource = RenderImageResource::SSAOBlur,
              .aspect = RenderImageAspect::Color0,
          },
      },
      {
          .label = "Bloom",
          .key = RenderImageExportKey{
              .resource = RenderImageResource::Bloom,
              .aspect = RenderImageAspect::Color0,
          },
      },
  }};
  bool m_show_attachments = false;
  bool m_show_grid = true;
  bool m_snap_enabled = false;
  uint64_t m_toolbar_version = 0u;
  uint64_t m_last_rendered_toolbar_version = 0u;
};

} // namespace astralix::editor
