#pragma once

#include "dsl.hpp"
#include "editor-theme.hpp"

#include <functional>
#include <string>

namespace astralix::editor::scene_hierarchy_panel {

ui::dsl::NodeSpec build_summary_card(
    ui::UINodeId &entity_count_node,
    ui::UINodeId &scene_name_node,
    ui::UINodeId &selection_text_node,
    ui::UINodeId &create_button_node,
    std::function<void()> on_create_click,
    const SceneHierarchyPanelTheme &theme
);

ui::dsl::NodeSpec build_search_input(
    ui::UINodeId &search_input_node,
    std::function<void(const std::string &)> on_change,
    const SceneHierarchyPanelTheme &theme
);

ui::dsl::NodeSpec
build_scroll_region(
    ui::UINodeId &scroll_node,
    std::function<void(const ui::UIPointerButtonEvent &)> on_secondary_click,
    const SceneHierarchyPanelTheme &theme
);

ui::dsl::NodeSpec build_empty_state(
    ui::UINodeId &empty_state_node,
    ui::UINodeId &empty_title_node,
    ui::UINodeId &empty_body_node,
    const SceneHierarchyPanelTheme &theme
);

} // namespace astralix::editor::scene_hierarchy_panel
