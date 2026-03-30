#pragma once

#include "systems/ui-system/core.hpp"

namespace astralix::ui_system_core {

void apply_item_control_visual_state(
    const std::vector<RootEntry> &roots,
    const std::optional<PointerHit> &hover_hit,
    const std::optional<Target> &active_target,
    ui::UIHitPart active_part,
    const std::optional<size_t> &active_item_index
);

} // namespace astralix::ui_system_core
