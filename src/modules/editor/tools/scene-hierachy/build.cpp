#include "tools/scene-hierachy/build.hpp"

#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include <utility>

namespace astralix::editor {

using namespace ui::dsl;
using namespace ui::dsl::styles;

namespace scene_hierarchy_panel {

ui::dsl::NodeSpec build_summary_card(
    ui::UINodeId &entity_count_node,
    ui::UINodeId &scene_name_node,
    ui::UINodeId &selection_text_node,
    const SceneHierarchyPanelTheme &theme
) {
  auto summary_header = row("scene_hierarchy_summary_header")
                            .style(fill_x().items_center().gap(12.0f));
  summary_header.child(
      column("scene_hierarchy_heading")
          .style(items_start().gap(3.0f))
          .children(
              text("Scene Hierarchy", "scene_hierarchy_title")
                  .style(font_size(18.0f).text_color(theme.text_primary)),
              text(
                  "Flat world listing for the active scene.",
                  "scene_hierarchy_copy"
              )
                  .style(font_size(12.5f).text_color(theme.text_muted))
          )
  );
  summary_header.child(spacer("scene_hierarchy_summary_spacer"));
  summary_header.child(
      text("0 entities", "scene_hierarchy_count")
          .bind(entity_count_node)
          .style(
              font_size(12.0f)
                  .text_color(theme.accent)
                  .padding_xy(12.0f, 8.0f)
                  .background(theme.accent_soft)
                  .border(1.0f, theme.accent)
                  .radius(999.0f)
          )
  );

  return column("scene_hierarchy_summary_card")
      .style(
          fill_x()
              .padding(16.0f)
              .gap(10.0f)
              .radius(18.0f)
              .background(theme.panel_background)
              .border(1.0f, theme.panel_border)
      )
      .children(
          std::move(summary_header),
          text("No active scene", "scene_hierarchy_scene_name")
              .bind(scene_name_node)
              .style(font_size(15.0f).text_color(theme.text_primary)),
          text("Selected: none", "scene_hierarchy_selection")
              .bind(selection_text_node)
              .style(font_size(12.5f).text_color(theme.text_muted))
      );
}

ui::dsl::NodeSpec build_search_input(
    ui::UINodeId &search_input_node,
    std::function<void(const std::string &)> on_change,
    const SceneHierarchyPanelTheme &theme
) {
  return text_input(
             {},
             "Search entities by name, tag, or id",
             "scene_hierarchy_search"
         )
      .bind(search_input_node)
      .select_all_on_focus(true)
      .on_change(std::move(on_change))
      .style(
          fill_x()
              .background(theme.card_background)
              .border(1.0f, theme.card_border)
      );
}

ui::dsl::NodeSpec
build_scroll_region(ui::UINodeId &scroll_node, const SceneHierarchyPanelTheme &theme) {
  return scroll_view("scene_hierarchy_scroll")
      .bind(scroll_node)
      .style(
          fill_x()
              .flex(1.0f)
              .padding(ui::UIEdges{
                  .left = 8.0f,
                  .top = 8.0f,
                  .right = 8.0f,
                  .bottom = 28.0f,
              })
              .gap(8.0f)
              .background(theme.panel_background)
              .border(1.0f, theme.panel_border)
              .radius(18.0f)
      );
}

ui::dsl::NodeSpec build_empty_state(
    ui::UINodeId &empty_state_node,
    ui::UINodeId &empty_title_node,
    ui::UINodeId &empty_body_node,
    const SceneHierarchyPanelTheme &theme
) {
  return column("scene_hierarchy_empty_state")
      .bind(empty_state_node)
      .style(
          fill()
              .justify_center()
              .items_center()
              .padding(24.0f)
              .gap(8.0f)
              .radius(18.0f)
              .background(theme.panel_background)
              .border(1.0f, theme.panel_border)
      )
      .visible(false)
      .children(
          text("No active scene", "scene_hierarchy_empty_title")
              .bind(empty_title_node)
              .style(font_size(18.0f).text_color(theme.text_primary)),
          text(
              "Scene entities appear here when SceneManager exposes an active world.",
              "scene_hierarchy_empty_body"
          )
              .bind(empty_body_node)
              .style(font_size(13.0f).text_color(theme.text_muted))
      );
}

} // namespace scene_hierarchy_panel

ui::dsl::NodeSpec SceneHierarchyPanelController::build() {
  const SceneHierarchyPanelTheme theme;

  return column("scene_hierarchy_root")
      .style(fill().background(theme.shell_background).padding(14.0f).gap(12.0f))
      .children(
          scene_hierarchy_panel::build_summary_card(
              m_entity_count_node,
              m_scene_name_node,
              m_selection_text_node,
              theme
          ),
          scene_hierarchy_panel::build_search_input(
              m_search_input_node,
              [this](const std::string &value) {
                m_search_query = value;
                refresh(true);
              },
              theme
          ),
          scene_hierarchy_panel::build_scroll_region(m_scroll_node, theme),
          scene_hierarchy_panel::build_empty_state(
              m_empty_state_node,
              m_empty_title_node,
              m_empty_body_node,
              theme
          )
      );
}

} // namespace astralix::editor
