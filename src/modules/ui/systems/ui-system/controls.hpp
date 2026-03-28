#pragma once

#include "systems/ui-system/core.hpp"

namespace astralix::ui_system_core {

bool toggle_checkbox_value(const Target &target, bool queue_callback);
bool apply_slider_value(const Target &target, float value, bool queue_change);
void update_slider_drag(const Target &target, glm::vec2 pointer);
bool select_segmented_option(const Target &target, size_t index,
                             bool queue_callback);
bool move_segmented_selection(const Target &target, int direction,
                              bool queue_callback);
bool toggle_chip(const Target &target, size_t index, bool queue_callback);
void move_select_highlight(const Target &target, int direction);
bool confirm_select_option(const Target &target, size_t index,
                           bool queue_callback);
void apply_select_visual_state(const std::vector<RootEntry> &roots,
                               const std::optional<PointerHit> &hover_hit);
void apply_item_control_visual_state(
    const std::vector<RootEntry> &roots,
    const std::optional<PointerHit> &hover_hit,
    const std::optional<Target> &active_target, ui::UIHitPart active_part,
    const std::optional<size_t> &active_item_index);

} // namespace astralix::ui_system_core
