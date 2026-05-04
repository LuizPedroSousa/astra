#include "tools/navigation-gizmo/navigation-gizmo-panel-controller.hpp"

#include "dsl/widgets/composites/button.hpp"
#include "editor-theme.hpp"
#include "editor-viewport-navigation-store.hpp"
#include "fnv1a.hpp"
#include "tools/navigation-gizmo/navigation-gizmo-shared.hpp"

namespace astralix::editor {
namespace {

using namespace ui::dsl;
using namespace ui::dsl::styles;
namespace nav = navigation_gizmo;

glm::vec4 blend_color(
    const glm::vec4 &lhs,
    const glm::vec4 &rhs,
    float factor
) {
  return lhs * (1.0f - factor) + rhs * factor;
}

ui::dsl::StyleBuilder body_style(const WorkspaceShellTheme &theme) {
  return width(px(nav::k_body_size))
      .height(px(nav::k_body_size))
      .shrink(0.0f)
      .radius(18.0f)
      .background(theme_alpha(theme.panel_background, 0.0f))
      .border(1.0f, theme_alpha(theme.panel_border, 0.92f))
      .overflow_hidden();
}

ui::dsl::StyleBuilder marker_hit_style(
    const WorkspaceShellTheme &theme,
    const nav::NavigationMarkerInstance &marker,
    const nav::NavigationMarkerProjection &projection
) {
  const glm::vec4 outline = marker.active
                                ? theme.text_primary
                                : blend_color(
                                      theme.panel_border,
                                      marker.config.color,
                                      marker.config.prominent ? 0.50f : 0.32f
                                  );

  return absolute()
      .left(px(projection.center.x - projection.size * 0.5f))
      .top(px(projection.center.y - projection.size * 0.5f))
      .width(px(projection.size))
      .height(px(projection.size))
      .padding(0.0f)
      .items_center()
      .justify_center()
      .radius(projection.size * 0.5f)
      .background(theme_alpha(marker.config.color, 0.0f))
      .border(1.0f, theme_alpha(outline, 0.0f))
      .cursor_pointer()
      .hover(
          state()
              .background(theme_alpha(marker.config.color, 0.12f))
              .border(1.0f, outline)
      )
      .pressed(
          state()
              .background(theme_alpha(marker.config.color, 0.18f))
              .border(1.0f, theme.text_primary)
      );
}

ui::dsl::StyleBuilder projection_button_style(
    const WorkspaceShellTheme &theme,
    bool orthographic
) {
  const glm::vec4 background =
      orthographic ? theme.accent_soft : theme.panel_raised_background;
  const glm::vec4 border =
      orthographic ? theme.accent : theme.panel_border;
  const glm::vec4 text =
      orthographic ? theme.text_primary : theme.text_muted;

  return fill_x()
      .height(px(nav::k_projection_button_height))
      .padding_xy(12.0f, 0.0f)
      .items_center()
      .justify_center()
      .radius(999.0f)
      .background(background)
      .border(1.0f, border)
      .text_color(text)
      .cursor_pointer()
      .hover(
          state()
              .background(blend_color(background, glm::vec4(1.0f), 0.06f))
              .border(1.0f, orthographic ? theme.text_primary : theme.accent)
      )
      .pressed(
          state()
              .background(
                  blend_color(background, theme.panel_background, 0.16f)
              )
              .border(1.0f, orthographic ? theme.text_primary : theme.accent)
      );
}

uint64_t append_camera_hash(uint64_t hash) {
  const auto selection = nav::active_viewport_camera_selection();
  hash = fnv1a64_append_value(hash, selection.has_value());
  if (!selection.has_value() || selection->camera == nullptr) {
    return hash;
  }

  const glm::vec3 forward = nav::normalized_or_fallback(
      selection->camera->front,
      glm::vec3(0.0f, 0.0f, -1.0f)
  );
  const glm::vec3 up = nav::normalized_or_fallback(
      selection->camera->up,
      glm::vec3(0.0f, 1.0f, 0.0f)
  );

  hash = fnv1a64_append_value(hash, selection->entity_id);
  hash = fnv1a64_append_value(hash, selection->camera->orthographic);
  hash = fnv1a64_append_value(hash, forward.x);
  hash = fnv1a64_append_value(hash, forward.y);
  hash = fnv1a64_append_value(hash, forward.z);
  hash = fnv1a64_append_value(hash, up.x);
  hash = fnv1a64_append_value(hash, up.y);
  hash = fnv1a64_append_value(hash, up.z);
  return hash;
}

} // namespace

void NavigationGizmoPanelController::render(ui::im::Frame &ui) {
  const WorkspaceShellTheme shell_theme;

  auto root = ui.column("root").style(
      fill()
          .items_center()
          .padding(10.0f)
          .gap(10.0f)
          .background(theme_alpha(shell_theme.panel_background, 0.0f))
  );

  auto body = root.view("body").style(body_style(shell_theme));
  m_body_widget = body.widget_id();

  if (const auto selection = nav::active_viewport_camera_selection();
      selection.has_value() && selection->camera != nullptr) {
    const ui::UIRect local_rect{
        .x = 0.0f,
        .y = 0.0f,
        .width = nav::k_body_size,
        .height = nav::k_body_size,
    };

    for (const auto &marker : nav::navigation_marker_instances(*selection)) {
      const auto projection = nav::project_marker_to_rect(marker, local_rect);
      auto hit =
          body.pressable(std::string(marker.config.id) + "_hit")
              .on_click([action = marker.config.action]() {
                editor_viewport_navigation_store()->request_action(action);
              })
              .style(marker_hit_style(shell_theme, marker, projection));
      hit.text("label", std::string(marker.config.label))
          .style(
              font_size(marker.config.prominent ? 12.5f : 8.0f)
                  .text_color(
                      marker.active ? shell_theme.text_primary
                                    : theme_alpha(
                                          shell_theme.text_primary,
                                          marker.config.prominent ? 0.92f
                                                                  : 0.74f
                                      )
                  )
          );
    }
  }

  const bool orthographic = [&]() {
    const auto selection = nav::active_viewport_camera_selection();
    return selection.has_value() && selection->camera != nullptr &&
           selection->camera->orthographic;
  }();

  root.button(
          "projection_toggle",
          orthographic ? "Orthographic" : "Perspective",
          []() {
            editor_viewport_navigation_store()->request_action(
                EditorViewportNavigationAction::ToggleProjection
            );
          }
      )
      .style(projection_button_style(shell_theme, orthographic));
}

void NavigationGizmoPanelController::mount(const PanelMountContext &context) {
  m_runtime = context.runtime;
  sync_draw_rect();
}

void NavigationGizmoPanelController::unmount() {
  m_runtime = nullptr;
  m_body_widget = ui::im::k_invalid_widget_id;
  editor_viewport_navigation_store()->set_draw_rect(std::nullopt);
}

void NavigationGizmoPanelController::update(const PanelUpdateContext &) {
  sync_draw_rect();
}

std::optional<uint64_t> NavigationGizmoPanelController::render_version() const {
  uint64_t hash = k_fnv1a64_offset_basis;
  hash = fnv1a64_append_value(
      hash,
      editor_viewport_navigation_store()->revision()
  );
  hash = append_camera_hash(hash);
  return hash;
}

void NavigationGizmoPanelController::sync_draw_rect() {
  if (m_runtime == nullptr || !m_body_widget) {
    editor_viewport_navigation_store()->set_draw_rect(std::nullopt);
    return;
  }

  auto bounds = m_runtime->layout_bounds(m_body_widget);
  if (!bounds.has_value()) {
    editor_viewport_navigation_store()->set_draw_rect(std::nullopt);
    return;
  }

  editor_viewport_navigation_store()->set_draw_rect(
      ui::inset_rect(*bounds, ui::UIEdges::all(8.0f))
  );
}

} // namespace astralix::editor
