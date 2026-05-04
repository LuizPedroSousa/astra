#include "tools/scene/styles.hpp"

namespace astralix::editor::scene_panel {

ui::dsl::StyleBuilder scene_menu_search_input_style(
    const ScenePanelTheme &theme
) {
  using namespace ui::dsl::styles;

  return fill_x()
      .height(px(50.0f))
      .padding_xy(14.0f, 10.0f)
      .background(theme.panel_background)
      .border(1.0f, theme.panel_border)
      .radius(12.0f)
      .text_color(theme.text_primary)
      .hover(state().background(theme.panel_raised_background))
      .focused(state().border(2.0f, theme.accent))
      .placeholder_text_color(theme.text_muted)
      .selection_color(
          glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.22f)
      )
      .caret_color(theme.text_primary);
}

ui::dsl::StyleBuilder separator_style(const ScenePanelTheme &theme) {
  using namespace ui::dsl::styles;

  return fill_x().height(px(1.0f)).background(theme.separator);
}

ui::dsl::StyleBuilder selector_button_style(const ScenePanelTheme &theme) {
  using namespace ui::dsl::styles;

  return row()
      .fill_x()
      .items_center()
      .justify_between()
      .height(px(28.0f))
      .padding_xy(8.0f, 0.0f)
      .radius(6.0f)
      .background(theme.panel_background)
      .border(1.0f, glm::vec4(theme.panel_border.r, theme.panel_border.g, theme.panel_border.b, 0.6f))
      .cursor_pointer()
      .hover(state().background(theme.panel_raised_background))
      .pressed(state().background(theme.accent_soft))
      .focused(state().border(2.0f, theme.accent));
}

ui::dsl::StyleBuilder pipeline_row_style() {
  using namespace ui::dsl::styles;

  return fill_x().items_center().gap(6.0f);
}

ui::dsl::StyleBuilder pipeline_pill_style(glm::vec4 status_color) {
  using namespace ui::dsl::styles;

  return padding_xy(6.0f, 3.0f)
      .radius(4.0f)
      .background(glm::vec4(status_color.r, status_color.g, status_color.b, 0.12f))
      .border(1.0f, glm::vec4(status_color.r, status_color.g, status_color.b, 0.30f))
      .items_center()
      .justify_center();
}

ui::dsl::StyleBuilder entity_pill_style(glm::vec4 color) {
  using namespace ui::dsl::styles;

  return padding_xy(6.0f, 3.0f)
      .radius(4.0f)
      .background(glm::vec4(color.r, color.g, color.b, 0.12f))
      .border(1.0f, glm::vec4(color.r, color.g, color.b, 0.40f))
      .items_center()
      .justify_center();
}

ui::dsl::StyleBuilder artifact_row_style() {
  using namespace ui::dsl::styles;

  return fill_x().items_center().gap(6.0f);
}

ui::dsl::StyleBuilder action_button_primary_style(const ScenePanelTheme &theme) {
  using namespace ui::dsl::styles;

  return fill_x()
      .height(px(32.0f))
      .items_center()
      .justify_center()
      .radius(6.0f)
      .background(glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.15f))
      .border(1.0f, glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.4f))
      .cursor_pointer()
      .hover(state().background(glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.22f)))
      .pressed(state().background(theme.accent_soft))
      .focused(state().border(2.0f, theme.accent))
      .disabled(state().opacity(0.72f));
}

ui::dsl::StyleBuilder action_button_secondary_style(const ScenePanelTheme &theme) {
  using namespace ui::dsl::styles;

  return grow(1.0f)
      .height(px(32.0f))
      .items_center()
      .justify_center()
      .radius(6.0f)
      .background(theme.panel_raised_background)
      .border(1.0f, glm::vec4(theme.panel_border.r, theme.panel_border.g, theme.panel_border.b, 0.6f))
      .cursor_pointer()
      .hover(state().background(theme.panel_background))
      .pressed(state().background(theme.accent_soft))
      .focused(state().border(2.0f, theme.accent))
      .disabled(state().opacity(0.72f));
}

ui::dsl::StyleBuilder status_bar_style(const ScenePanelTheme &theme) {
  using namespace ui::dsl::styles;

  return fill_x().background(theme.status_bar_background);
}

} // namespace astralix::editor::scene_panel
