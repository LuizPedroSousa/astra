#pragma once

#include "dsl.hpp"
#include "editor-theme.hpp"
#include "managers/scene-manager.hpp"
#include "tools/scene/helpers.hpp"

#include <functional>
#include <optional>
#include <string>

namespace astralix::editor::scene_panel {

ui::dsl::StyleBuilder scene_menu_search_input_style(
    const ScenePanelTheme &theme
);
ui::dsl::StyleBuilder lifecycle_action_button_style(
    const ScenePanelTheme &theme,
    bool emphasized = false
);

ui::dsl::NodeSpec build_lifecycle_action_button(
    std::string label,
    bool enabled,
    bool emphasized,
    ui::UINodeId &button_node,
    std::function<void()> on_click,
    const ScenePanelTheme &theme
);

ui::dsl::NodeSpec build_scene_menu_trigger_button(
    std::string label,
    ui::UINodeId &button_node,
    ui::UINodeId &icon_node,
    ui::UINodeId &label_node,
    bool open,
    std::function<void()> on_click,
    const ScenePanelTheme &theme
);

ui::dsl::NodeSpec build_scene_mode_toggle(
    std::optional<SceneSessionKind> active_session_kind,
    bool enabled,
    ui::UINodeId &toggle_node,
    std::function<void(size_t, const std::string &)> on_select,
    const ScenePanelTheme &theme
);

ui::dsl::NodeSpec build_scene_menu_popover(
    ui::UINodeId &popover_node,
    ui::UINodeId &search_input_node,
    ui::UINodeId &result_count_node,
    ui::UINodeId &list_node,
    ui::UINodeId &content_node,
    ui::UINodeId &empty_state_node,
    ui::UINodeId &empty_title_node,
    ui::UINodeId &empty_body_node,
    const std::string &query,
    std::function<void(const std::string &)> on_search_change,
    const ScenePanelTheme &theme
);

ui::dsl::NodeSpec build_scene_menu_entry_item(
    const ProjectSceneEntryConfig &entry,
    bool active,
    const SceneMenuEntryPresentation &presentation,
    std::function<void()> on_activate_scene,
    const ScenePanelTheme &theme
);

ui::dsl::NodeSpec build_scene_status_card(
    std::string scene_label,
    SceneSourceSaveState source_state,
    ScenePreviewState preview_state,
    SceneRuntimeState runtime_state,
    SceneExecutionState active_execution_state,
    std::string runtime_hint,
    bool playback_controls_enabled,
    ui::UINodeId &execution_chip_node,
    ui::UINodeId &execution_value_node,
    ui::UINodeId &playback_controls_node,
    bool play_enabled,
    ui::UINodeId &play_button_node,
    std::function<void()> on_play_click,
    bool pause_enabled,
    ui::UINodeId &pause_button_node,
    std::function<void()> on_pause_click,
    bool stop_enabled,
    ui::UINodeId &stop_button_node,
    std::function<void()> on_stop_click,
    bool save_enabled,
    ui::UINodeId &save_button_node,
    std::function<void()> on_save_click,
    bool promote_enabled,
    ui::UINodeId &promote_button_node,
    std::function<void()> on_promote_click,
    ui::UINodeId &card_node,
    ui::UINodeId &scene_value_node,
    ui::UINodeId &source_chip_node,
    ui::UINodeId &source_value_node,
    ui::UINodeId &preview_chip_node,
    ui::UINodeId &preview_value_node,
    ui::UINodeId &runtime_chip_node,
    ui::UINodeId &runtime_value_node,
    ui::UINodeId &runtime_hint_node,
    const ScenePanelTheme &theme
);

ui::dsl::NodeSpec build_runtime_prompt(
    ui::UINodeId &popover_node,
    ui::UINodeId &title_node,
    ui::UINodeId &body_node,
    std::function<void()> on_build_and_enter,
    std::function<void()> on_cancel,
    const ScenePanelTheme &theme
);

} // namespace astralix::editor::scene_panel
