#pragma once

#include "systems/ui-system/core.hpp"

namespace astralix::ui_system_core {

std::optional<ScrollDispatch>
find_scroll_dispatch(const std::vector<RootEntry> &roots, const Target &deepest_target, const ui::UIMouseWheelInputEvent &event);
void page_scroll_track(const Target &target, ui::UIHitPart part, glm::vec2 pointer);
void update_scrollbar_drag(const Target &target, ui::UIHitPart part, float grab_offset, glm::vec2 pointer);
void apply_scrollbar_visual_state(
    const std::vector<RootEntry> &roots,
    const std::optional<PointerHit> &hover_hit,
    const std::optional<std::pair<Target, ui::UIHitPart>> &active_scrollbar
);

} // namespace astralix::ui_system_core
