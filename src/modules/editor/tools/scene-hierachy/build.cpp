#include "tools/scene-hierachy/build.hpp"

#include "dsl/widgets/composites/button.hpp"
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
    ui::UINodeId &create_button_node,
    std::function<void()> on_create_click,
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
  auto summary_actions =
      row("scene_hierarchy_summary_actions").style(items_center().gap(10.0f));
  summary_actions.child(
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
  summary_actions.child(
      button("Create", std::move(on_create_click), "scene_hierarchy_create_button")
          .bind(create_button_node)
          .style(
              background(theme.accent_soft)
                  .border(1.0f, theme.accent)
                  .radius(999.0f)
                  .padding_xy(14.0f, 8.0f)
                  .hover(state().background(theme.accent))
                  .pressed(state().background(theme.panel_background))
          )
  );
  summary_header.child(
      std::move(summary_actions)
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
build_scroll_region(
    ui::UINodeId &scroll_node,
    std::function<void(const ui::UIPointerButtonEvent &)> on_secondary_click,
    const SceneHierarchyPanelTheme &theme
) {
  return scroll_view("scene_hierarchy_scroll")
      .bind(scroll_node)
      .on_secondary_click(std::move(on_secondary_click))
      .style(
          fill_x()
              .flex()
              .gap(14.0f)
      );
}

ui::dsl::NodeSpec build_menu_popover(
    ui::UINodeId &popover_node,
    std::string name,
    const SceneHierarchyPanelTheme &theme
) {
  return popover(std::move(name))
      .bind(popover_node)
      .style(
          items_start()
              .gap(6.0f)
              .width(px(250.0f))
              .padding(12.0f)
              .background(theme.card_background)
              .border(1.0f, theme.card_border)
              .radius(12.0f)
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

  auto create_menu = scene_hierarchy_panel::build_menu_popover(
      m_create_menu_node,
      "scene_hierarchy_create_menu",
      theme
  );

  create_menu.children(
      menu_item(
          "Create Empty",
          [this]() { create_empty_entity(); },
          "scene_hierarchy_create_empty"
      )
          .on_hover([this]() {
            if (m_document == nullptr) {
              return;
            }

            m_document->close_popovers_from_depth(1u);
          }),
      menu_separator("scene_hierarchy_create_menu_separator"),
      submenu_item(
          "3D Object",
          [this]() {
            if (m_document == nullptr) {
              return;
            }

            m_document->open_popover_anchored_to(
                m_create_3d_menu_node,
                m_create_3d_trigger_node,
                ui::UIPopupPlacement::RightStart,
                1u
            );
          },
          "scene_hierarchy_create_3d_trigger"
      )
          .bind(m_create_3d_trigger_node),
      submenu_item(
          "Light",
          [this]() {
            if (m_document == nullptr) {
              return;
            }

            m_document->open_popover_anchored_to(
                m_create_light_menu_node,
                m_create_light_trigger_node,
                ui::UIPopupPlacement::RightStart,
                1u
            );
          },
          "scene_hierarchy_create_light_trigger"
      )
          .bind(m_create_light_trigger_node)
  );

  auto create_3d_menu = scene_hierarchy_panel::build_menu_popover(
      m_create_3d_menu_node,
      "scene_hierarchy_create_3d_menu",
      theme
  );

  create_3d_menu.children(
      menu_item("Cube", [this]() {
        create_mesh_primitive("Cube", Mesh::cube(2.0f));
      }),
      menu_item("Sphere", [this]() {
        create_mesh_primitive("Sphere", Mesh::sphere());
      }),
      menu_item("Capsule", [this]() {
        create_mesh_primitive("Capsule", Mesh::capsule());
      }),
      menu_item("Quad", [this]() {
        create_mesh_primitive("Quad", Mesh::quad());
      }),
      menu_item("Plane", [this]() {
        create_mesh_primitive("Plane", Mesh::plane());
      })
  );

  auto create_light_menu = scene_hierarchy_panel::build_menu_popover(
      m_create_light_menu_node,
      "scene_hierarchy_create_light_menu",
      theme
  );
  create_light_menu.children(
      menu_item("Directional", [this]() {
        create_light_entity(
            "Directional Light",
            rendering::LightType::Directional
        );
      }),
      menu_item("Point", [this]() {
        create_light_entity("Point Light", rendering::LightType::Point);
      }),
      menu_item("Spot", [this]() {
        create_light_entity("Spot Light", rendering::LightType::Spot);
      })
  );

  auto row_menu = scene_hierarchy_panel::build_menu_popover(
      m_row_menu_node,
      "scene_hierarchy_row_menu",
      theme
  );
  row_menu.children(
      submenu_item(
          "Add Component",
          [this]() {
            if (m_document != nullptr) {
              m_document->open_popover_anchored_to(
                  m_row_add_component_menu_node,
                  m_row_add_component_trigger_node,
                  ui::UIPopupPlacement::RightStart,
                  1u
              );
            }
          },
          "scene_hierarchy_row_add_component_trigger"
      )
          .bind(m_row_add_component_trigger_node),
      menu_separator("scene_hierarchy_row_menu_separator"),
      menu_item(
          "Delete",
          [this]() { delete_context_entity(); },
          "scene_hierarchy_row_delete"
      )
          .on_hover([this]() {
            if (m_document != nullptr) {
              m_document->close_popovers_from_depth(1u);
            }
          })
  );

  auto row_add_component_menu = scene_hierarchy_panel::build_menu_popover(
      m_row_add_component_menu_node,
      "scene_hierarchy_row_add_component_menu",
      theme
  );
  row_add_component_menu.children(
      column("scene_hierarchy_row_add_component_container")
          .bind(m_row_add_component_container_node)
          .style(fill_x().items_start().gap(4.0f))
  );

  return column("scene_hierarchy_root")
      .style(
          fill()
              .background(theme.shell_background)
              .padding(14.0f)
              .gap(12.0f)
      )
      .children(
          scene_hierarchy_panel::build_summary_card(
              m_entity_count_node,
              m_scene_name_node,
              m_selection_text_node,
              m_create_button_node,
              [this]() { open_create_menu_anchored(); },
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
          scene_hierarchy_panel::build_scroll_region(
              m_scroll_node,
              [this](const ui::UIPointerButtonEvent &event) {
                open_create_menu_at(event.position);
              },
              theme
          ),
          scene_hierarchy_panel::build_empty_state(
              m_empty_state_node,
              m_empty_title_node,
              m_empty_body_node,
              theme
          )
              .on_secondary_click(
                  [this](const ui::UIPointerButtonEvent &event) {
                    open_create_menu_at(event.position);
                  }
              ),
          std::move(create_menu),
          std::move(create_3d_menu),
          std::move(create_light_menu),
          std::move(row_menu),
          std::move(row_add_component_menu)
      );
}

} // namespace astralix::editor
