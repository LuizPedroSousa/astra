#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include "editor-theme.hpp"
#include "trace.hpp"

namespace astralix::editor {
namespace {

using namespace ui::dsl::styles;

ui::dsl::StyleBuilder panel_menu_style(const SceneHierarchyPanelTheme &theme) {
  return items_start()
      .gap(6.0f)
      .width(px(250.0f))
      .padding(12.0f)
      .background(theme.card_background)
      .border(1.0f, theme.card_border)
      .radius(12.0f);
}

ui::dsl::StyleBuilder menu_item_style(
    const SceneHierarchyPanelTheme &theme,
    bool enabled = true
) {
  auto style = row()
                   .fill_x()
                   .items_center()
                   .justify_between()
                   .gap(8.0f)
                   .padding_xy(10.0f, 8.0f)
                   .radius(8.0f)
                   .border(1.0f, theme_alpha(theme.card_border, 0.0f))
                   .background(theme_alpha(theme.panel_background, 0.0f));
  if (enabled) {
    style.cursor_pointer()
        .hover(
            state()
                .background(theme_alpha(theme.panel_background, 0.72f))
                .border(1.0f, theme_alpha(theme.card_border, 0.32f))
        )
        .pressed(state().background(theme_alpha(theme.panel_background, 0.92f)))
        .focused(state().border(1.0f, theme.accent));
  }
  return style;
}

ui::dsl::StyleBuilder toolbar_create_button_style(
    const SceneHierarchyPanelTheme &theme
) {
  return items_center()
      .justify_center()
      .width(px(28.0f))
      .height(px(28.0f))
      .padding(0.0f)
      .radius(8.0f)
      .background(theme.accent_soft)
      .border(1.0f, theme_alpha(theme.accent, 0.52f))
      .text_color(theme.accent)
      .cursor_pointer()
      .hover(
          state()
              .background(theme_alpha(theme.accent, 0.24f))
              .border(1.0f, theme.accent)
      )
      .pressed(state().background(theme_alpha(theme.accent, 0.32f)))
      .focused(state().border(2.0f, theme.accent));
}

} // namespace

void SceneHierarchyPanelController::render(ui::im::Frame &ui) {
  ASTRA_PROFILE_N("SceneHierarchyPanel::render");
  const SceneHierarchyPanelTheme theme;
  const EntityEntry *selection = selected_entry();
  const std::string scene_name =
      m_has_scene ? m_scene_name : std::string("No active scene");
  const std::string entity_count = scene_hierarchy_panel::entity_count_label(
      m_entities.size(), m_all_entities.size()
  );

  auto root = ui.column("root").style(
      fill().background(theme.shell_background).padding(14.0f).gap(12.0f)
  );

  {
  ASTRA_PROFILE_N("SceneHierarchyPanel::toolbar");
  auto toolbar = root.column("toolbar").style(fill_x().gap(6.0f));
  auto title_row = toolbar.row("title-row").style(fill_x().items_center().gap(8.0f));
  title_row.text("scene-name", scene_name)
      .style(font_size(16.0f).text_color(theme.text_primary));
  title_row.spacer("spacer");
  title_row.text("entity-count", entity_count)
      .style(
          font_size(11.0f)
              .text_color(theme.text_muted)
              .padding_xy(8.0f, 3.0f)
              .background(theme_alpha(theme.card_background, 0.56f))
              .border(1.0f, theme_alpha(theme.card_border, 0.42f))
              .radius(999.0f)
      );
  auto create_button = title_row.pressable("create-button")
                           .enabled(m_has_scene)
                           .on_click([this]() { open_create_menu_anchored(); })
                           .style(toolbar_create_button_style(theme));
  create_button.text("label", "+")
      .style(font_size(18.0f).text_color(theme.accent));
  m_create_button_widget = create_button.widget_id();

  if (auto selection_row =
      toolbar.row("selection-row")
          .visible(selection != nullptr)
          .style(fill_x().items_center().gap(6.0f))) {
    selection_row.text("label", "Selected")
        .style(font_size(11.0f).text_color(theme_alpha(theme.text_muted, 0.72f)));
    selection_row.text(
        "value",
        selection->name + " (#" + static_cast<std::string>(selection->id) + ")"
    )
        .style(
            font_size(11.0f)
                .text_color(theme.text_muted)
                .padding_xy(6.0f, 2.0f)
                .background(theme_alpha(theme.card_background, 0.42f))
                .border(1.0f, theme_alpha(theme.card_border, 0.32f))
                .radius(4.0f)
        );
  }

  }

  root.text_input("search", m_search_query, "Search by name, type, scope, or id")
      .select_all_on_focus(true)
      .on_change([this](const std::string &value) {
        m_search_query = value;
        close_menus();
        refresh(true);
      })
      .style(
          fill_x()
              .background(theme.card_background)
              .border(1.0f, theme.card_border)
              .font_id(m_default_font_id)
              .font_size(std::max(13.0f, m_default_font_size * 0.78f))
      );

  const bool has_visible_rows = m_has_scene && !m_visible_rows.empty();
  if (auto rows_container = root.view("rows-container").visible(has_visible_rows)) {
    ASTRA_PROFILE_N("SceneHierarchyPanel::visible_rows");
    rows_container.style(fill());
    render_visible_rows(rows_container);
  }

  if (auto empty = root.column("empty-state").visible(!has_visible_rows)
                     .on_secondary_click([this](const ui::UIPointerButtonEvent &event) {
                       open_create_menu_at(event.position);
                     })
                     .style(
                         fill()
                             .justify_center()
                             .items_center()
                             .padding(24.0f)
                             .gap(8.0f)
                             .radius(18.0f)
                             .background(theme.panel_background)
                             .border(1.0f, theme.panel_border)
                     )) {
    empty.text("title", m_empty_title)
        .style(font_size(18.0f).text_color(theme.text_primary));
    empty.text("body", m_empty_body)
        .style(font_size(13.0f).text_color(theme.text_muted));
  }

  {
  ASTRA_PROFILE_N("SceneHierarchyPanel::menus");
  auto create_menu =
      static_cast<ui::im::Children &>(root).popover("create-menu").popover(
          ui::im::PopoverState{
              .open = m_create_menu_open,
              .anchor_widget_id =
                  m_create_menu_anchor_point.has_value()
                      ? ui::im::k_invalid_widget_id
                      : m_create_button_widget,
              .anchor_point = m_create_menu_anchor_point,
              .placement = ui::UIPopupPlacement::BottomStart,
              .depth = 0u,
              .close_on_outside_click = false,
              .close_on_escape = false,
          }
      );
  create_menu.style(panel_menu_style(theme));
  create_menu.pressable("create-empty")
      .on_click([this]() { create_empty_entity(); })
      .style(menu_item_style(theme))
      .text("label", "Create Empty")
      .style(font_size(13.0f).text_color(theme.text_primary));
  create_menu.view("separator-1")
      .style(fill_x().height(px(1.0f)).background(theme_alpha(theme.card_border, 0.42f)));

  auto create_3d_trigger =
      create_menu.pressable("create-3d")
          .on_click([this]() {
            m_create_3d_menu_open = !m_create_3d_menu_open;
            m_create_light_menu_open = false;
            mark_render_dirty();
          })
          .style(menu_item_style(theme));
  create_3d_trigger.text("label", "3D Object")
      .style(font_size(13.0f).text_color(theme.text_primary));
  create_3d_trigger.text("chevron", ">")
      .style(font_size(11.0f).text_color(theme.text_muted));
  m_create_3d_trigger_widget = create_3d_trigger.widget_id();

  auto create_light_trigger =
      create_menu.pressable("create-light")
          .on_click([this]() {
            m_create_light_menu_open = !m_create_light_menu_open;
            m_create_3d_menu_open = false;
            mark_render_dirty();
          })
          .style(menu_item_style(theme));
  create_light_trigger.text("label", "Light")
      .style(font_size(13.0f).text_color(theme.text_primary));
  create_light_trigger.text("chevron", ">")
      .style(font_size(11.0f).text_color(theme.text_muted));
  m_create_light_trigger_widget = create_light_trigger.widget_id();

  auto create_3d_menu =
      static_cast<ui::im::Children &>(root)
          .popover("create-3d-menu")
          .popover(ui::im::PopoverState{
              .open = m_create_3d_menu_open,
              .anchor_widget_id = m_create_3d_trigger_widget,
              .placement = ui::UIPopupPlacement::RightStart,
              .depth = 1u,
              .close_on_outside_click = false,
              .close_on_escape = false,
          });
  create_3d_menu.style(panel_menu_style(theme));
  create_3d_menu.pressable("cube")
      .on_click([this]() { create_mesh_primitive("Cube", Mesh::cube(2.0f)); })
      .style(menu_item_style(theme))
      .text("label", "Cube")
      .style(font_size(13.0f).text_color(theme.text_primary));
  create_3d_menu.pressable("sphere")
      .on_click([this]() { create_mesh_primitive("Sphere", Mesh::sphere()); })
      .style(menu_item_style(theme))
      .text("label", "Sphere")
      .style(font_size(13.0f).text_color(theme.text_primary));
  create_3d_menu.pressable("capsule")
      .on_click([this]() { create_mesh_primitive("Capsule", Mesh::capsule()); })
      .style(menu_item_style(theme))
      .text("label", "Capsule")
      .style(font_size(13.0f).text_color(theme.text_primary));
  create_3d_menu.pressable("quad")
      .on_click([this]() { create_mesh_primitive("Quad", Mesh::quad()); })
      .style(menu_item_style(theme))
      .text("label", "Quad")
      .style(font_size(13.0f).text_color(theme.text_primary));
  create_3d_menu.pressable("plane")
      .on_click([this]() { create_mesh_primitive("Plane", Mesh::plane()); })
      .style(menu_item_style(theme))
      .text("label", "Plane")
      .style(font_size(13.0f).text_color(theme.text_primary));

  auto create_light_menu =
      static_cast<ui::im::Children &>(root)
          .popover("create-light-menu")
          .popover(ui::im::PopoverState{
              .open = m_create_light_menu_open,
              .anchor_widget_id = m_create_light_trigger_widget,
              .placement = ui::UIPopupPlacement::RightStart,
              .depth = 1u,
              .close_on_outside_click = false,
              .close_on_escape = false,
          });
  create_light_menu.style(panel_menu_style(theme));
  create_light_menu.pressable("directional")
      .on_click([this]() {
        create_light_entity("Directional Light", rendering::LightType::Directional);
      })
      .style(menu_item_style(theme))
      .text("label", "Directional")
      .style(font_size(13.0f).text_color(theme.text_primary));
  create_light_menu.pressable("point")
      .on_click([this]() {
        create_light_entity("Point Light", rendering::LightType::Point);
      })
      .style(menu_item_style(theme))
      .text("label", "Point")
      .style(font_size(13.0f).text_color(theme.text_primary));
  create_light_menu.pressable("spot")
      .on_click([this]() {
        create_light_entity("Spot Light", rendering::LightType::Spot);
      })
      .style(menu_item_style(theme))
      .text("label", "Spot")
      .style(font_size(13.0f).text_color(theme.text_primary));

  auto row_menu =
      static_cast<ui::im::Children &>(root).popover("row-menu").popover(
          ui::im::PopoverState{
              .open = m_row_menu_open,
              .anchor_point = m_row_menu_anchor_point,
              .placement = ui::UIPopupPlacement::BottomStart,
              .depth = 0u,
              .close_on_outside_click = false,
              .close_on_escape = false,
          }
      );
  row_menu.style(panel_menu_style(theme));
  auto add_component_trigger =
      row_menu.pressable("add-component")
          .enabled(!m_add_component_options.empty())
          .on_click([this]() {
            if (!m_add_component_options.empty()) {
              m_row_add_component_menu_open = !m_row_add_component_menu_open;
              mark_render_dirty();
            }
          })
          .style(menu_item_style(theme, !m_add_component_options.empty()));
  add_component_trigger.text("label", "Add Component")
      .style(
          font_size(13.0f).text_color(
              m_add_component_options.empty() ? theme.text_muted : theme.text_primary
          )
      );
  add_component_trigger.text("chevron", ">")
      .visible(!m_add_component_options.empty())
      .style(font_size(11.0f).text_color(theme.text_muted));
  m_row_add_component_trigger_widget = add_component_trigger.widget_id();

  row_menu.view("separator-1")
      .style(fill_x().height(px(1.0f)).background(theme_alpha(theme.card_border, 0.42f)));
  row_menu.pressable("delete")
      .on_click([this]() { delete_context_entity(); })
      .style(menu_item_style(theme))
      .text("label", "Delete")
      .style(font_size(13.0f).text_color(theme.text_primary));

  auto add_component_menu =
      static_cast<ui::im::Children &>(root)
          .popover("row-add-component-menu")
          .popover(ui::im::PopoverState{
              .open = m_row_add_component_menu_open && !m_add_component_options.empty(),
              .anchor_widget_id = m_row_add_component_trigger_widget,
              .placement = ui::UIPopupPlacement::RightStart,
              .depth = 1u,
              .close_on_outside_click = false,
              .close_on_escape = false,
          });
  add_component_menu.style(panel_menu_style(theme));
  for (size_t index = 0u; index < m_add_component_options.size(); ++index) {
    const std::string &label = m_add_component_options[index];
    add_component_menu.pressable(std::string("option-") + std::to_string(index))
        .on_click([this, label]() {
          const auto it = m_add_component_lookup.find(label);
          if (it != m_add_component_lookup.end()) {
            add_component_to_context_entity(it->second);
          }
        })
        .style(menu_item_style(theme))
        .text("label", label)
        .style(font_size(13.0f).text_color(theme.text_primary));
  }
  }
}

} // namespace astralix::editor
