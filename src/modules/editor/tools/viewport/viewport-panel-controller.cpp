#include "tools/viewport/viewport-panel-controller.hpp"

#include "dsl/widgets/composites/button.hpp"
#include "dsl/widgets/layout/column.hpp"
#include "dsl/widgets/layout/scroll-view.hpp"
#include "editor-theme.hpp"
#include "tools/viewport/gizmo-math.hpp"

namespace astralix::editor {

using namespace ui::dsl;
using namespace ui::dsl::styles;

namespace {

ui::dsl::NodeSpec build_attachment_preview(
    const std::string &label,
    RenderImageExportKey render_image_key,
    ui::UINodeId &label_node,
    ui::UINodeId &image_node,
    std::function<void()> on_click,
    const ViewportPanelTheme &theme,
    const WorkspaceShellTheme &shell_theme
) {
  return pressable()
      .on_click(std::move(on_click))
      .style(
          ui::dsl::styles::column()
              .width(px(160.0f))
              .gap(6.0f)
              .shrink(0.0f)
              .padding(8.0f)
              .background(shell_theme.panel_background)
              .border(1.0f, shell_theme.panel_border)
              .radius(14.0f)
              .cursor_pointer()
              .hover(
                  state()
                      .background(shell_theme.accent_soft)
                      .border(1.0f, shell_theme.accent)
              )
              .pressed(
                  state()
                      .background(shell_theme.panel_raised_background)
                      .border(1.0f, shell_theme.accent)
              )
      )
      .children(
          text(label)
              .bind(label_node)
              .style(font_size(11.0f).text_color(shell_theme.text_muted)),
          render_image_view(render_image_key)
              .bind(image_node)
              .style(
                  background(theme.surface)
                      .fill_x()
                      .height(px(120.0f))
                      .border(1.0f, shell_theme.panel_border)
                      .radius(12.0f)
                      .overflow_hidden()
              )
      );
}

ui::dsl::StateStyleRule build_toggle_hover_style(
    const WorkspaceShellTheme &shell_theme
) {
  return state()
      .background(shell_theme.accent_soft)
      .border(1.0f, shell_theme.accent);
}

ui::dsl::StyleBuilder build_toolbar_pill(
    bool active,
    const WorkspaceShellTheme &shell_theme
) {
  return background(active ? shell_theme.accent_soft
                           : theme_alpha(shell_theme.panel_background, 0.0f))
      .border(1.0f, active ? shell_theme.accent : shell_theme.panel_border)
      .radius(10.0f)
      .padding_xy(14.0f, 6.0f)
      .text_color(active ? shell_theme.text_primary : shell_theme.text_muted)
      .hover(build_toggle_hover_style(shell_theme))
      .pressed(
          state()
              .background(shell_theme.panel_background)
              .border(1.0f, shell_theme.accent)
      );
}

} // namespace

ui::dsl::NodeSpec ViewportPanelController::build() {
  const ViewportPanelTheme theme;
  const WorkspaceShellTheme shell_theme;

  return ui::dsl::column()
      .style(
          fill()
              .background(theme.surface)
              .gap(0.0f)
              .padding(0.0f)
              .overflow_hidden()
      )
      .children(
          ui::dsl::column()
              .style(
                  fill_x()
                      .gap(6.0f)
                      .padding(10.0f)
                      .background(shell_theme.panel_background)
              )
              .children(
                  ui::dsl::row()
                      .style(fill_x().items_center().gap(8.0f))
                      .children(
                          segmented_control(
                              {"Translate", "Rotate", "Scale"},
                              gizmo::mode_to_index(editor_gizmo_store()->mode())
                          )
                              .accent_colors({
                                  glm::vec4(0.90f, 0.25f, 0.25f, 1.0f),
                                  glm::vec4(0.25f, 0.80f, 0.25f, 1.0f),
                                  glm::vec4(0.30f, 0.50f, 0.95f, 1.0f),
                              })
                              .bind(m_mode_toggle_node)
                              .style(
                                  width(px(310.0f)),
                                  background(theme.surface),
                                  border(1.0f, shell_theme.panel_border),
                                  radius(10.0f)
                              )
                              .on_select([this](size_t index, const std::string &) {
                                set_mode(gizmo::mode_from_index(index));
                              }),
                          view().style(
                              width(px(1.0f))
                                  .height(px(22.0f))
                                  .background(shell_theme.panel_border)
                          ),
                          button(
                              "Grid",
                              [this]() { toggle_grid(); }
                          )
                              .bind(m_grid_toggle_node)
                              .style(build_toolbar_pill(m_show_grid, shell_theme)),
                          button(
                              "Snap",
                              [this]() { toggle_snap(); }
                          )
                              .bind(m_snap_toggle_node)
                              .style(build_toolbar_pill(m_snap_enabled, shell_theme)),
                          spacer(),
                          button(
                              m_show_attachments ? "Hide Buffers" : "Buffers",
                              [this]() { toggle_attachments(); }
                          )
                              .bind(m_attachments_toggle_node)
                              .style(build_toolbar_pill(m_show_attachments, shell_theme)),
                          view().style(
                              width(px(1.0f))
                                  .height(px(22.0f))
                                  .background(shell_theme.panel_border)
                          ),
                          text("View:")
                              .style(font_size(11.0f).text_color(shell_theme.text_muted)),
                          text("Scene")
                              .bind(m_view_label_node)
                              .style(font_size(11.0f).text_color(shell_theme.text_primary))
                      )
              ),
          scroll_view()
              .bind(m_attachments_strip_node)
              .style(
                  fill_x()
                      .height(px(170.0f))
                      .background(shell_theme.panel_background)
                      .border(1.0f, shell_theme.panel_border)
                      .radius(14.0f)
                      .padding(10.0f)
                      .overflow_hidden()
                      .scrollbar_auto()
                      .raw([](ui::UIStyle &style) {
                        style.scroll_mode = ui::ScrollMode::Horizontal;
                        style.scrollbar_thickness = 8.0f;
                      })
              )
              .visible(m_show_attachments)
              .child(
                  ui::dsl::row()
                      .style(
                          gap(10.0f)
                              .items_start()
                      )
                      .children(
                          build_attachment_preview(
                              m_attachment_views[0].label,
                              m_attachment_views[0].key,
                              m_attachment_label_nodes[0],
                              m_attachment_image_nodes[0],
                              [this]() { swap_attachment_into_main(0u); },
                              theme,
                              shell_theme
                          ),
                          build_attachment_preview(
                              m_attachment_views[1].label,
                              m_attachment_views[1].key,
                              m_attachment_label_nodes[1],
                              m_attachment_image_nodes[1],
                              [this]() { swap_attachment_into_main(1u); },
                              theme,
                              shell_theme
                          ),
                          build_attachment_preview(
                              m_attachment_views[2].label,
                              m_attachment_views[2].key,
                              m_attachment_label_nodes[2],
                              m_attachment_image_nodes[2],
                              [this]() { swap_attachment_into_main(2u); },
                              theme,
                              shell_theme
                          ),
                          build_attachment_preview(
                              m_attachment_views[3].label,
                              m_attachment_views[3].key,
                              m_attachment_label_nodes[3],
                              m_attachment_image_nodes[3],
                              [this]() { swap_attachment_into_main(3u); },
                              theme,
                              shell_theme
                          ),
                          build_attachment_preview(
                              m_attachment_views[4].label,
                              m_attachment_views[4].key,
                              m_attachment_label_nodes[4],
                              m_attachment_image_nodes[4],
                              [this]() { swap_attachment_into_main(4u); },
                              theme,
                              shell_theme
                          ),
                          build_attachment_preview(
                              m_attachment_views[5].label,
                              m_attachment_views[5].key,
                              m_attachment_label_nodes[5],
                              m_attachment_image_nodes[5],
                              [this]() { swap_attachment_into_main(5u); },
                              theme,
                              shell_theme
                          ),
                          build_attachment_preview(
                              m_attachment_views[6].label,
                              m_attachment_views[6].key,
                              m_attachment_label_nodes[6],
                              m_attachment_image_nodes[6],
                              [this]() { swap_attachment_into_main(6u); },
                              theme,
                              shell_theme
                          ),
                          build_attachment_preview(
                              m_attachment_views[7].label,
                              m_attachment_views[7].key,
                              m_attachment_label_nodes[7],
                              m_attachment_image_nodes[7],
                              [this]() { swap_attachment_into_main(7u); },
                              theme,
                              shell_theme
                          )
                      )
              ),
          ui::dsl::column()
              .style(
                  fill_x()
                      .flex(1.0f)
                      .min_height(px(0.0f))
                      .background(shell_theme.panel_background)
                      .border(1.0f, shell_theme.panel_border)
                      .radius(14.0f)
                      .overflow_hidden()
              )
              .child(
                  render_image_view(
                      RenderImageResource::SceneColor,
                      RenderImageAspect::Color0
                  )
                      .bind(m_viewport_image_node)
                      .style(
                          fill()
                              .background(theme.surface)
                      )
              )
      );
}

void ViewportPanelController::mount(const PanelMountContext &context) {
  m_document = context.document;
  sync_mode_ui();
  sync_attachments_ui();
  sync_render_views_ui();
  sync_panel_rect();
}

void ViewportPanelController::unmount() {
  m_document = nullptr;
  m_mode_toggle_node = ui::k_invalid_node_id;
  m_attachments_toggle_node = ui::k_invalid_node_id;
  m_attachments_strip_node = ui::k_invalid_node_id;
  m_view_label_node = ui::k_invalid_node_id;
  m_viewport_image_node = ui::k_invalid_node_id;
  m_grid_toggle_node = ui::k_invalid_node_id;
  m_snap_toggle_node = ui::k_invalid_node_id;
  m_attachment_label_nodes.fill(ui::k_invalid_node_id);
  m_attachment_image_nodes.fill(ui::k_invalid_node_id);
  editor_gizmo_store()->set_panel_rect(std::nullopt);
}

void ViewportPanelController::update(const PanelUpdateContext &) {
  sync_mode_ui();
  sync_attachments_ui();
  sync_grid_ui();
  sync_snap_ui();
  sync_render_views_ui();
  sync_panel_rect();
}

void ViewportPanelController::set_mode(EditorGizmoMode mode) {
  editor_gizmo_store()->set_mode(mode);
  sync_mode_ui();
}

void ViewportPanelController::toggle_attachments() {
  m_show_attachments = !m_show_attachments;
  sync_attachments_ui();
}

void ViewportPanelController::toggle_grid() {
  m_show_grid = !m_show_grid;
  sync_grid_ui();
}

void ViewportPanelController::toggle_snap() {
  m_snap_enabled = !m_snap_enabled;
  sync_snap_ui();
}

void ViewportPanelController::swap_attachment_into_main(size_t index) {
  if (index >= m_attachment_views.size()) {
    return;
  }

  std::swap(m_main_view, m_attachment_views[index]);
  sync_render_views_ui();
}

void ViewportPanelController::sync_mode_ui() {
  if (m_document == nullptr || m_mode_toggle_node == ui::k_invalid_node_id) {
    return;
  }

  m_document->set_segmented_selected_index(
      m_mode_toggle_node,
      gizmo::mode_to_index(editor_gizmo_store()->mode())
  );
}

void ViewportPanelController::sync_attachments_ui() {
  if (m_document == nullptr) {
    return;
  }

  const WorkspaceShellTheme shell_theme;

  if (m_attachments_strip_node != ui::k_invalid_node_id) {
    m_document->set_visible(m_attachments_strip_node, m_show_attachments);
  }

  if (m_attachments_toggle_node != ui::k_invalid_node_id) {
    m_document->set_text(
        m_attachments_toggle_node,
        m_show_attachments ? "Hide Attachments" : "Attachments"
    );
    m_document->mutate_style(
        m_attachments_toggle_node,
        [this, shell_theme](ui::UIStyle &style) {
          style.background_color = m_show_attachments
                                       ? shell_theme.accent_soft
                                       : theme_alpha(
                                             shell_theme.panel_background, 0.0f
                                         );
          style.border_color = m_show_attachments ? shell_theme.accent
                                                  : shell_theme.panel_border;
          style.text_color = shell_theme.text_primary;
        }
    );
  }
}

void ViewportPanelController::sync_grid_ui() {
  if (m_document == nullptr || m_grid_toggle_node == ui::k_invalid_node_id) {
    return;
  }

  const WorkspaceShellTheme shell_theme;
  m_document->set_text(m_grid_toggle_node, "Grid");
  m_document->mutate_style(
      m_grid_toggle_node,
      [this, shell_theme](ui::UIStyle &style) {
        style.background_color = m_show_grid
                                     ? shell_theme.accent_soft
                                     : theme_alpha(
                                           shell_theme.panel_background, 0.0f
                                       );
        style.border_color =
            m_show_grid ? shell_theme.accent : shell_theme.panel_border;
        style.text_color =
            m_show_grid ? shell_theme.text_primary : shell_theme.text_muted;
      }
  );
}

void ViewportPanelController::sync_snap_ui() {
  if (m_document == nullptr || m_snap_toggle_node == ui::k_invalid_node_id) {
    return;
  }

  const WorkspaceShellTheme shell_theme;
  m_document->set_text(m_snap_toggle_node, "Snap");
  m_document->mutate_style(
      m_snap_toggle_node,
      [this, shell_theme](ui::UIStyle &style) {
        style.background_color = m_snap_enabled
                                     ? shell_theme.accent_soft
                                     : theme_alpha(
                                           shell_theme.panel_background, 0.0f
                                       );
        style.border_color =
            m_snap_enabled ? shell_theme.accent : shell_theme.panel_border;
        style.text_color =
            m_snap_enabled ? shell_theme.text_primary : shell_theme.text_muted;
      }
  );
}

void ViewportPanelController::sync_render_views_ui() {
  if (m_document == nullptr) {
    return;
  }

  if (m_view_label_node != ui::k_invalid_node_id) {
    m_document->set_text(m_view_label_node, m_main_view.label);
  }

  if (m_viewport_image_node != ui::k_invalid_node_id) {
    m_document->set_render_image_key(m_viewport_image_node, m_main_view.key);
  }

  for (size_t index = 0; index < m_attachment_views.size(); ++index) {
    if (m_attachment_label_nodes[index] != ui::k_invalid_node_id) {
      m_document->set_text(
          m_attachment_label_nodes[index], m_attachment_views[index].label
      );
    }

    if (m_attachment_image_nodes[index] != ui::k_invalid_node_id) {
      m_document->set_render_image_key(
          m_attachment_image_nodes[index], m_attachment_views[index].key
      );
    }
  }
}

void ViewportPanelController::sync_panel_rect() {
  if (m_document == nullptr || m_viewport_image_node == ui::k_invalid_node_id) {
    editor_gizmo_store()->set_panel_rect(std::nullopt);
    return;
  }

  const auto *node = m_document->node(m_viewport_image_node);
  if (node == nullptr || !node->visible || node->layout.bounds.width <= 0.0f ||
      node->layout.bounds.height <= 0.0f) {
    editor_gizmo_store()->set_panel_rect(std::nullopt);
    return;
  }

  editor_gizmo_store()->set_panel_rect(node->layout.bounds);
}

} // namespace astralix::editor
