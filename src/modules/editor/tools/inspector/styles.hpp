#pragma once

#include "dsl.hpp"
#include "editor-theme.hpp"

namespace astralix::editor::inspector_panel {

ui::dsl::StyleBuilder component_card_style(const InspectorPanelTheme &theme);
ui::dsl::StyleBuilder input_field_style(const InspectorPanelTheme &theme);
ui::dsl::StyleBuilder compact_button_style(const InspectorPanelTheme &theme);
ui::dsl::StyleBuilder remove_button_style(const InspectorPanelTheme &theme);
ui::dsl::StyleBuilder checkbox_field_style(const InspectorPanelTheme &theme);

} // namespace astralix::editor::inspector_panel
