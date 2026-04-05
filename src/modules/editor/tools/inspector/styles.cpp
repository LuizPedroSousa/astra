#include "styles.hpp"

namespace astralix::editor {
namespace panel = inspector_panel;

using namespace ui::dsl;
using namespace ui::dsl::styles;

ui::dsl::StyleBuilder panel::component_card_style(
    const InspectorPanelTheme &theme
) {
  return fill_x()
      .padding(4.0f)
      .gap(8.0f)
      .radius(12.0f)
      .background(theme.card_background)
      .border(1.0f, theme.card_border);
}

ui::dsl::StyleBuilder panel::input_field_style(
    const InspectorPanelTheme &theme
) {
  return fill_x()
      .padding_xy(8.0f, 6.0f)
      .font_size(12.5f)
      .control_gap(5.0f)
      .control_indicator_size(14.0f)
      .background(theme.input_background)
      .border(1.0f, theme.input_border)
      .radius(8.0f);
}

ui::dsl::StyleBuilder panel::compact_button_style(
    const InspectorPanelTheme &theme
) {
  return padding_xy(10.0f, 5.0f)
      .background(theme.accent_soft)
      .border(1.0f, theme.accent)
      .radius(8.0f);
}

ui::dsl::StyleBuilder panel::remove_button_style(
    const InspectorPanelTheme &theme
) {
  return padding_xy(10.0f, 5.0f)
      .background(theme.remove_background)
      .border(1.0f, theme.remove_border)
      .radius(8.0f);
}

ui::dsl::StyleBuilder panel::checkbox_field_style(
    const InspectorPanelTheme &theme
) {
  return fill_x()
      .padding_xy(0.0f, 4.0f)
      .font_size(12.5f)
      .text_color(theme.text_primary)
      .control_gap(5.0f)
      .control_indicator_size(14.0f);
}

} // namespace astralix::editor
