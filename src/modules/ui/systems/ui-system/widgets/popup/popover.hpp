#pragma once

#include "systems/ui-system/core.hpp"

namespace astralix::ui_system_core {

bool point_hits_open_popover(
    const std::vector<RootEntry> &roots,
    glm::vec2 point
);

bool close_popovers_on_outside_press(
    const std::vector<RootEntry> &roots,
    glm::vec2 point
);

bool close_popovers_on_escape(
    const std::vector<RootEntry> &roots,
    const ui::UIKeyInputEvent &event
);

std::optional<Target> find_secondary_click_target(
    const RootEntry &entry,
    ui::UINodeId node_id,
    ui::UIHitPart part
);

} // namespace astralix::ui_system_core
