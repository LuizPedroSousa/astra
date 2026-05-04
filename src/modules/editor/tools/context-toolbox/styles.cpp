#include "tools/context-toolbox/styles.hpp"

namespace astralix::editor::context_toolbox_styles {

using namespace ui::dsl::styles;

ui::dsl::StyleBuilder root_style(const ContextToolboxTheme &theme) {
  return fill()
      .items_center()
      .gap(2.0f)
      .padding(ui::UIEdges{
          .left = 4.0f,
          .top = 4.0f,
          .right = 0.0f,
          .bottom = 4.0f,
      })
      .scroll_vertical()
      .scrollbar_auto()
      .scrollbar_thickness(6.0f);
}

ui::dsl::StyleBuilder button_style(
    const ContextToolboxTheme &theme,
    bool active
) {
  return items_center()
      .justify_center()
      .width(px(40.0f))
      .height(px(40.0f))
      .padding(0.0f)
      .radius(10.0f)
      .background(active ? theme.button_active : theme.button_background)
      .border(
          1.0f,
          active ? theme_alpha(theme.button_active_border, 0.56f)
                 : theme.button_border
      )
      .cursor_pointer()
      .overflow_hidden()
      .hover(
          state()
              .background(active ? theme.button_active : theme.button_hover)
              .border(
                  1.0f,
                  active ? theme.button_active_border
                         : theme_alpha(theme.button_active_border, 0.32f)
              )
      )
      .pressed(
          state()
              .background(theme.button_active)
              .border(1.0f, theme.button_active_border)
      )
      .focused(state().border(2.0f, theme.button_active_border));
}

ui::dsl::StyleBuilder active_indicator_style(
    const ContextToolboxTheme &theme
) {
  return absolute()
      .left(px(0.0f))
      .top(px(8.0f))
      .bottom(px(8.0f))
      .width(px(2.0f))
      .background(theme.button_active_border)
      .radius(999.0f);
}

ui::dsl::StyleBuilder icon_style(
    const ContextToolboxTheme &theme,
    bool active
) {
  return width(px(18.0f))
      .height(px(18.0f))
      .tint(active ? theme.icon_tint_active : theme.icon_tint);
}

ui::dsl::StyleBuilder group_separator_style(
    const ContextToolboxTheme &theme
) {
  return width(px(18.0f))
      .height(px(1.0f))
      .background(theme.group_separator);
}

ui::dsl::StyleBuilder tooltip_style(const ContextToolboxTheme &theme) {
  return items_start()
      .gap(6.0f)
      .padding_xy(10.0f, 8.0f)
      .background(theme.tooltip_background)
      .border(1.0f, theme.tooltip_border)
      .radius(10.0f);
}

ui::dsl::StyleBuilder shortcut_badge_style(
    const ContextToolboxTheme &theme
) {
  return padding_xy(6.0f, 2.0f)
      .background(theme.shortcut_background)
      .border(1.0f, theme.shortcut_border)
      .radius(999.0f)
      .text_color(theme.shortcut_text);
}

} // namespace astralix::editor::context_toolbox_styles
