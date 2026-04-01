#pragma once

#include "systems/ui-system/core.hpp"

namespace astralix::ui_system_core {

void move_select_highlight(const Target &target, int direction);
bool confirm_select_option(const Target &target, size_t index, bool queue_callback);
void apply_select_visual_state(
    const std::vector<RootEntry> &roots,
    const std::optional<PointerHit> &hover_hit
);

} // namespace astralix::ui_system_core
