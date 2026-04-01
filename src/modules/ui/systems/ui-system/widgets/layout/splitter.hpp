#pragma once

#include "systems/ui-system/ui-system.hpp"

namespace astralix::ui_system_core {

std::optional<UISystem::SplitterResizeDrag>
begin_splitter_resize_drag(const Target &target, glm::vec2 pointer);

void update_splitter_resize_drag(
    const UISystem::SplitterResizeDrag &drag,
    glm::vec2 pointer
);

} // namespace astralix::ui_system_core
