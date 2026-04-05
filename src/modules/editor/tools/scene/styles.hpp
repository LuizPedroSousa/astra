#pragma once

#include "dsl.hpp"
#include "editor-theme.hpp"
namespace astralix::editor::scene_panel {

ui::dsl::StyleBuilder scene_menu_search_input_style(
    const ScenePanelTheme &theme
);
ui::dsl::StyleBuilder lifecycle_action_button_style(
    const ScenePanelTheme &theme,
    bool emphasized = false
);

} // namespace astralix::editor::scene_panel
