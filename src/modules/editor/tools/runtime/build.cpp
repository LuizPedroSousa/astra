#include "tools/runtime/build.hpp"

#include "tools/runtime/runtime-panel-controller.hpp"

#include <utility>

namespace astralix::editor {

using namespace ui::dsl;
using namespace ui::dsl::styles;

namespace {

StyleBuilder summary_card_style(const RuntimePanelTheme &theme) {
  return flex(1.0f)
      .padding(16.0f)
      .gap(10.0f)
      .radius(18.0f)
      .background(theme.panel_background)
      .border(1.0f, theme.panel_border)
      .items_start()
      .raw([](ui::UIStyle &style) {
        style.min_width = ui::UILength::pixels(180.0f);
      });
}

StyleBuilder metric_card_style(const RuntimePanelTheme &theme) {
  return flex(1.0f)
      .padding(14.0f)
      .gap(8.0f)
      .radius(16.0f)
      .background(theme.card_background)
      .border(1.0f, theme.card_border)
      .items_start()
      .raw([](ui::UIStyle &style) {
        style.min_width = ui::UILength::pixels(124.0f);
      });
}

NodeSpec section_heading(
    std::string name,
    const char *title,
    const char *body,
    const RuntimePanelTheme &theme
) {
  return column(std::move(name))
      .style(items_start().gap(2.0f))
      .children(
          text(title, "title")
              .style(font_size(14.0f).text_color(theme.text_primary)),
          text(body, "body")
              .style(font_size(12.5f).text_color(theme.text_muted))
      );
}

NodeSpec summary_card(
    std::string name,
    const char *label,
    const char *body,
    ui::UINodeId &value_node,
    const glm::vec4 &value_color,
    const RuntimePanelTheme &theme
) {
  return column(std::move(name))
      .style(summary_card_style(theme))
      .children(
          text(label, "label")
              .style(font_size(12.5f).text_color(theme.text_muted)),
          text("--", "value")
              .bind(value_node)
              .style(font_size(32.0f).text_color(value_color)),
          text(body, "body")
              .style(font_size(12.0f).text_color(theme.text_muted))
      );
}

NodeSpec metric_card(
    std::string name,
    const char *label,
    const char *body,
    ui::UINodeId &value_node,
    const glm::vec4 &value_color,
    const RuntimePanelTheme &theme
) {
  return column(std::move(name))
      .style(metric_card_style(theme))
      .children(
          text(label, "label")
              .style(font_size(12.0f).text_color(theme.text_muted)),
          text("--", "value")
              .bind(value_node)
              .style(font_size(24.0f).text_color(value_color)),
          text(body, "body")
              .style(font_size(11.5f).text_color(theme.text_muted))
      );
}

} // namespace

namespace runtime_panel {

ui::dsl::NodeSpec build_header(
    ui::UINodeId &scene_status_chip_node,
    ui::UINodeId &scene_status_text_node,
    ui::UINodeId &scene_name_node,
    const RuntimePanelTheme &theme
) {
  return row("runtime_header")
      .style(fill_x().items_center().gap(12.0f))
      .children(
          column("runtime_header_copy")
              .style(items_start().gap(3.0f))
              .children(
                  text("Runtime", "runtime_title")
                      .style(font_size(20.0f).text_color(theme.text_primary)),
                  text(
                      "Live counters for the active scene, renderer, physics, and UI state",
                      "runtime_subtitle"
                  )
                      .style(font_size(13.0f).text_color(theme.text_muted))
              ),
          spacer("runtime_header_spacer"),
          column("runtime_scene_meta")
              .style(items_end().gap(6.0f))
              .children(
                  row("runtime_scene_status_chip")
                      .bind(scene_status_chip_node)
                      .style(
                          items_center()
                              .padding_xy(12.0f, 8.0f)
                              .radius(999.0f)
                              .background(theme.success_soft)
                              .border(1.0f, theme.success)
                      )
                      .children(
                          text("ACTIVE", "runtime_scene_status")
                              .bind(scene_status_text_node)
                              .style(
                                  font_size(12.5f).text_color(theme.success)
                              )
                      ),
                  text("No active scene", "runtime_scene_name")
                      .bind(scene_name_node)
                      .style(font_size(13.0f).text_color(theme.text_muted))
              )
      );
}

ui::dsl::NodeSpec build_summary_row(
    ui::UINodeId &fps_value_node,
    ui::UINodeId &frame_time_value_node,
    const RuntimePanelTheme &theme
) {
  return row("runtime_summary_row")
      .style(fill_x().gap(12.0f))
      .children(
          summary_card(
              "runtime_fps_card",
              "Frame Rate",
              "Average FPS over the last 0.25s.",
              fps_value_node,
              theme.accent,
              theme
          ),
          summary_card(
              "runtime_frame_time_card",
              "Frame Time",
              "Average milliseconds per frame.",
              frame_time_value_node,
              theme.text_primary,
              theme
          )
      );
}

ui::dsl::NodeSpec build_scene_section(
    ui::UINodeId &entities_value_node,
    ui::UINodeId &renderables_value_node,
    const RuntimePanelTheme &theme
) {
  auto cards = row("runtime_scene_cards").style(fill_x().gap(12.0f));
  cards.child(
      metric_card(
          "runtime_entities_card",
          "Entities",
          "Scene entities tracked by the world.",
          entities_value_node,
          theme.text_primary,
          theme
      )
  );
  cards.child(
      metric_card(
          "runtime_renderables_card",
          "Renderables",
          "Objects currently tagged for rendering.",
          renderables_value_node,
          theme.accent,
          theme
      )
  );

  return column("runtime_scene_section")
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "runtime_scene_heading",
              "Scene",
              "High-level footprint of the active world.",
              theme
          ),
          std::move(cards)
      );
}

ui::dsl::NodeSpec build_physics_section(
    ui::UINodeId &rigid_bodies_value_node,
    ui::UINodeId &dynamic_bodies_value_node,
    ui::UINodeId &static_bodies_value_node,
    const RuntimePanelTheme &theme
) {
  auto cards = row("runtime_physics_cards").style(fill_x().gap(12.0f));
  cards.child(
      metric_card(
          "runtime_rigid_bodies_card",
          "Rigid Bodies",
          "All bodies registered with physics.",
          rigid_bodies_value_node,
          theme.text_primary,
          theme
      )
  );
  cards.child(
      metric_card(
          "runtime_dynamic_bodies_card",
          "Dynamic",
          "Bodies simulated every frame.",
          dynamic_bodies_value_node,
          theme.accent,
          theme
      )
  );
  cards.child(
      metric_card(
          "runtime_static_bodies_card",
          "Static",
          "Bodies used as immovable collision.",
          static_bodies_value_node,
          theme.text_primary,
          theme
      )
  );

  return column("runtime_physics_section")
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "runtime_physics_heading",
              "Physics",
              "Rigid-body totals by simulation mode.",
              theme
          ),
          std::move(cards)
      );
}

ui::dsl::NodeSpec build_systems_section(
    ui::UINodeId &lights_value_node,
    ui::UINodeId &cameras_value_node,
    ui::UINodeId &ui_roots_value_node,
    const RuntimePanelTheme &theme
) {
  auto cards = row("runtime_systems_cards").style(fill_x().gap(12.0f));
  cards.child(
      metric_card(
          "runtime_lights_card",
          "Lights",
          "Directional, point, and spot lights.",
          lights_value_node,
          theme.accent,
          theme
      )
  );
  cards.child(
      metric_card(
          "runtime_cameras_card",
          "Main Cameras",
          "Active view-driving camera tags.",
          cameras_value_node,
          theme.text_primary,
          theme
      )
  );
  cards.child(
      metric_card(
          "runtime_ui_roots_card",
          "UI Roots",
          "Top-level UI documents in the world.",
          ui_roots_value_node,
          theme.text_primary,
          theme
      )
  );

  return column("runtime_systems_section")
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "runtime_systems_heading",
              "Systems",
              "Core runtime subsystems visible in the scene.",
              theme
          ),
          std::move(cards)
      );
}

ui::dsl::NodeSpec
build_empty_state(ui::UINodeId &empty_state_node, const RuntimePanelTheme &theme) {
  return column("runtime_empty_state")
      .bind(empty_state_node)
      .style(
          fill_x()
              .padding(18.0f)
              .gap(6.0f)
              .radius(18.0f)
              .background(theme.panel_background)
              .border(1.0f, theme.panel_border)
              .items_start()
      )
      .visible(false)
      .children(
          text("No active scene", "runtime_empty_state_title")
              .style(font_size(18.0f).text_color(theme.text_primary)),
          text(
              "Runtime counters appear here when SceneManager exposes an active scene.",
              "runtime_empty_state_body"
          )
              .style(font_size(13.0f).text_color(theme.text_muted))
      );
}

} // namespace runtime_panel

ui::dsl::NodeSpec RuntimePanelController::build() {
  const RuntimePanelTheme theme;

  return scroll_view("runtime_scroll")
      .style(fill().background(theme.shell_background))
      .child(
          column("runtime_root")
              .style(fill_x().padding(18.0f).gap(14.0f))
              .children(
                  runtime_panel::build_header(
                      m_scene_status_chip_node,
                      m_scene_status_text_node,
                      m_scene_name_node,
                      theme
                  ),
                  runtime_panel::build_summary_row(
                      m_fps_value_node, m_frame_time_value_node, theme
                  ),
                  column("runtime_metrics_root")
                      .bind(m_metrics_root_node)
                      .style(fill_x().gap(12.0f))
                      .children(
                          runtime_panel::build_scene_section(
                              m_entities_value_node,
                              m_renderables_value_node,
                              theme
                          ),
                          runtime_panel::build_physics_section(
                              m_rigid_bodies_value_node,
                              m_dynamic_bodies_value_node,
                              m_static_bodies_value_node,
                              theme
                          ),
                          runtime_panel::build_systems_section(
                              m_lights_value_node,
                              m_cameras_value_node,
                              m_ui_roots_value_node,
                              theme
                          )
                      ),
                  runtime_panel::build_empty_state(m_empty_state_node, theme)
              )
      );
}

} // namespace astralix::editor
