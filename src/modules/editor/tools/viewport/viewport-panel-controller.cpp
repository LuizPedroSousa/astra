#include "tools/viewport/viewport-panel-controller.hpp"

#include "dsl/widgets/composites/button.hpp"
#include "dsl/widgets/layout/column.hpp"
#include "dsl/widgets/layout/scroll-view.hpp"
#include "editor-theme.hpp"
#include "fnv1a.hpp"
#include "tools/viewport/gizmo-math.hpp"
#include "trace.hpp"

namespace astralix::editor {

using namespace ui::dsl;
using namespace ui::dsl::styles;

namespace {

void draw_attachment_preview(
    ui::im::Children &parent,
    std::string_view local_name,
    const std::string &label,
    RenderImageExportKey render_image_key,
    std::function<void()> on_click,
    const ViewportPanelTheme &theme,
    const WorkspaceShellTheme &shell_theme
) {
  auto preview = parent.pressable(local_name)
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
                                     .background(
                                         shell_theme.panel_raised_background
                                     )
                                     .border(1.0f, shell_theme.accent)
                             )
                     );
  preview.text("label", label)
      .style(font_size(11.0f).text_color(shell_theme.text_muted));
  preview.render_image_view("image", render_image_key)
      .style(
          background(theme.surface)
              .fill_x()
              .height(px(120.0f))
              .border(1.0f, shell_theme.panel_border)
              .radius(12.0f)
              .overflow_hidden()
      );
}

ui::dsl::StateStyleBuilder build_toggle_hover_style(
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

void ViewportPanelController::render(ui::im::Frame &ui) {
  ASTRA_PROFILE_N("ViewportPanel::render");
  const ViewportPanelTheme theme;
  const WorkspaceShellTheme shell_theme;

  auto root = ui.column("root").style(
      fill()
          .background(theme.surface)
          .gap(0.0f)
          .padding(0.0f)
          .overflow_hidden()
  );

  if (auto toolbar = root.column("toolbar")
          .frozen(m_toolbar_version == m_last_rendered_toolbar_version)
          .style(
              fill_x()
                  .gap(6.0f)
                  .padding(10.0f)
                  .background(shell_theme.panel_background)
          )) {
    m_last_rendered_toolbar_version = m_toolbar_version;
    ASTRA_PROFILE_N("ViewportPanel::toolbar");
    auto controls = toolbar.row("controls").style(fill_x().items_center().gap(8.0f));
    controls
        .segmented_control(
            "mode",
            {"Translate", "Rotate", "Scale"},
            gizmo::mode_to_index(editor_gizmo_store()->mode())
        )
        .accent_colors({
            glm::vec4(0.90f, 0.25f, 0.25f, 1.0f),
            glm::vec4(0.25f, 0.80f, 0.25f, 1.0f),
            glm::vec4(0.30f, 0.50f, 0.95f, 1.0f),
        })
        .style(
            width(px(310.0f)),
            background(theme.surface),
            border(1.0f, shell_theme.panel_border),
            radius(10.0f)
        )
        .on_select([this](size_t index, const std::string &) {
          set_mode(gizmo::mode_from_index(index));
        });
    controls.view("divider_left")
        .style(
            width(px(1.0f))
                .height(px(22.0f))
                .background(shell_theme.panel_border)
        );
    controls.button("grid", "Grid", [this]() { toggle_grid(); })
        .style(build_toolbar_pill(m_show_grid, shell_theme));
    controls.button("snap", "Snap", [this]() { toggle_snap(); })
        .style(build_toolbar_pill(m_snap_enabled, shell_theme));
    controls.spacer("toolbar_spacer");
    controls
        .button(
            "attachments_toggle",
            m_show_attachments ? "Hide Buffers" : "Buffers",
            [this]() { toggle_attachments(); }
        )
        .style(build_toolbar_pill(m_show_attachments, shell_theme));
    controls.view("divider_right")
        .style(
            width(px(1.0f))
                .height(px(22.0f))
                .background(shell_theme.panel_border)
        );
    controls.text("view_label_prefix", "View:")
        .style(font_size(11.0f).text_color(shell_theme.text_muted));
    controls.text("view_label", m_main_view.label)
        .style(font_size(11.0f).text_color(shell_theme.text_primary));
  }

  if (auto attachments = root.scroll_view("attachments").visible(m_show_attachments).style(
      fill_x()
          .height(px(170.0f))
          .background(shell_theme.panel_background)
          .border(1.0f, shell_theme.panel_border)
          .radius(14.0f)
          .padding(10.0f)
          .overflow_hidden()
          .scrollbar_auto()
          .scroll_horizontal()
          .scrollbar_thickness(8.0f)
  )) {
    ASTRA_PROFILE_N("ViewportPanel::attachments");
    auto items = attachments.row("items").style(gap(10.0f).items_start());
    for (size_t index = 0u; index < m_attachment_views.size(); ++index) {
      draw_attachment_preview(
          items,
          "attachment_" + std::to_string(index),
          m_attachment_views[index].label,
          m_attachment_views[index].key,
          [this, index]() { swap_attachment_into_main(index); },
          theme,
          shell_theme
      );
    }
  }

  {
  ASTRA_PROFILE_N("ViewportPanel::viewport_shell");
  auto viewport_shell = root.column("viewport_shell").style(
      fill_x()
          .flex(1.0f)
          .min_height(px(0.0f))
          .background(shell_theme.panel_background)
          .border(1.0f, shell_theme.panel_border)
          .radius(14.0f)
          .overflow_hidden()
  );
  auto image = viewport_shell.render_image_view("image", m_main_view.key).style(
      fill().background(theme.surface)
  );
  m_viewport_image_widget = image.widget_id();
  }
}

void ViewportPanelController::mount(const PanelMountContext &context) {
  m_runtime = context.runtime;
  sync_panel_rect();
}

std::optional<uint64_t> ViewportPanelController::render_version() const {
  uint64_t hash = k_fnv1a64_offset_basis;
  hash = fnv1a64_append_value(hash, editor_gizmo_store()->mode());
  hash = fnv1a64_append_value(hash, m_show_attachments);
  hash = fnv1a64_append_value(hash, m_show_grid);
  hash = fnv1a64_append_value(hash, m_snap_enabled);
  hash = fnv1a64_append_string(m_main_view.label, hash);
  hash = fnv1a64_append_value(hash, m_main_view.key);

  for (const RenderViewSlot &slot : m_attachment_views) {
    hash = fnv1a64_append_string(slot.label, hash);
    hash = fnv1a64_append_value(hash, slot.key);
  }

  return hash;
}

void ViewportPanelController::unmount() {
  m_runtime = nullptr;
  m_viewport_image_widget = ui::im::k_invalid_widget_id;
  editor_gizmo_store()->set_panel_rect(std::nullopt);
}

void ViewportPanelController::update(const PanelUpdateContext &) { sync_panel_rect(); }

void ViewportPanelController::set_mode(EditorGizmoMode mode) {
  editor_gizmo_store()->set_mode(mode);
  ++m_toolbar_version;
}

void ViewportPanelController::toggle_attachments() {
  m_show_attachments = !m_show_attachments;
  ++m_toolbar_version;
}

void ViewportPanelController::toggle_grid() {
  m_show_grid = !m_show_grid;
  ++m_toolbar_version;
}

void ViewportPanelController::toggle_snap() {
  m_snap_enabled = !m_snap_enabled;
  ++m_toolbar_version;
}

void ViewportPanelController::swap_attachment_into_main(size_t index) {
  if (index >= m_attachment_views.size()) {
    return;
  }

  std::swap(m_main_view, m_attachment_views[index]);
}

void ViewportPanelController::sync_panel_rect() {
  if (m_runtime != nullptr && m_viewport_image_widget) {
    editor_gizmo_store()->set_panel_rect(
        m_runtime->layout_bounds(m_viewport_image_widget)
    );
    return;
  }
  editor_gizmo_store()->set_panel_rect(std::nullopt);
}

} // namespace astralix::editor
