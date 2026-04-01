#pragma once

#include "editor-gizmo-store.hpp"
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
  ui::dsl::NodeSpec build() override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;

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
  void sync_mode_ui();
  void sync_attachments_ui();
  void sync_grid_ui();
  void sync_snap_ui();
  void sync_render_views_ui();
  void sync_panel_rect();

  Ref<ui::UIDocument> m_document = nullptr;
  ui::UINodeId m_mode_toggle_node = ui::k_invalid_node_id;
  ui::UINodeId m_attachments_toggle_node = ui::k_invalid_node_id;
  ui::UINodeId m_attachments_strip_node = ui::k_invalid_node_id;
  ui::UINodeId m_view_label_node = ui::k_invalid_node_id;
  ui::UINodeId m_viewport_image_node = ui::k_invalid_node_id;
  ui::UINodeId m_grid_toggle_node = ui::k_invalid_node_id;
  ui::UINodeId m_snap_toggle_node = ui::k_invalid_node_id;
  std::array<ui::UINodeId, kAttachmentViewCount> m_attachment_label_nodes{};
  std::array<ui::UINodeId, kAttachmentViewCount> m_attachment_image_nodes{};
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
};

} // namespace astralix::editor
