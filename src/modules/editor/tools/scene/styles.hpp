#pragma once

#include "dsl.hpp"
#include "editor-theme.hpp"

#include <glm/glm.hpp>

namespace astralix::editor::scene_panel {

ui::dsl::StyleBuilder scene_menu_search_input_style(
    const ScenePanelTheme &theme
);
ui::dsl::StyleBuilder separator_style(const ScenePanelTheme &theme);
ui::dsl::StyleBuilder selector_button_style(const ScenePanelTheme &theme);
ui::dsl::StyleBuilder pipeline_row_style();
ui::dsl::StyleBuilder pipeline_pill_style(glm::vec4 status_color);
ui::dsl::StyleBuilder entity_pill_style(glm::vec4 color);
ui::dsl::StyleBuilder artifact_row_style();
ui::dsl::StyleBuilder action_button_primary_style(const ScenePanelTheme &theme);
ui::dsl::StyleBuilder action_button_secondary_style(const ScenePanelTheme &theme);
ui::dsl::StyleBuilder status_bar_style(const ScenePanelTheme &theme);

} // namespace astralix::editor::scene_panel
