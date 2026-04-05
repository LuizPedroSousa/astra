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

ui::dsl::StyleBuilder lifecycle_action_button_style(
    const ScenePanelTheme &theme,
    bool emphasized
) {
  using namespace ui::dsl::styles;

  return padding_xy(14.0f, 9.0f)
      .radius(12.0f)
      .background(emphasized ? theme.accent_soft : theme.panel_background)
      .border(1.0f, emphasized ? theme.accent : theme.panel_border)
      .text_color(theme.text_primary)
      .cursor_pointer()
      .hover(
          state().background(
              emphasized ? theme.accent_soft : theme.panel_raised_background
          )
      )
      .pressed(state().background(theme.accent_soft))
      .focused(state().border(2.0f, theme.accent))
      .disabled(state().opacity(0.45f));
}

} // namespace astralix::editor::scene_panel
