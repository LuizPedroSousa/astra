#pragma once

#include "dsl.hpp"
#include "editor-theme.hpp"

namespace astralix::editor::runtime_panel {

ui::dsl::NodeSpec build_header(
    ui::UINodeId &scene_status_chip_node,
    ui::UINodeId &scene_status_text_node,
    ui::UINodeId &scene_name_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_summary_row(
    ui::UINodeId &fps_value_node,
    ui::UINodeId &frame_time_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_scene_section(
    ui::UINodeId &entities_value_node,
    ui::UINodeId &renderables_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_physics_section(
    ui::UINodeId &rigid_bodies_value_node,
    ui::UINodeId &dynamic_bodies_value_node,
    ui::UINodeId &static_bodies_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_systems_section(
    ui::UINodeId &lights_value_node,
    ui::UINodeId &cameras_value_node,
    ui::UINodeId &ui_roots_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec
build_empty_state(ui::UINodeId &empty_state_node, const RuntimePanelTheme &theme);

} // namespace astralix::editor::runtime_panel
