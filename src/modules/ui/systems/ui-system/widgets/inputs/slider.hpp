#pragma once

#include "systems/ui-system/core.hpp"

namespace astralix::ui_system_core {

bool apply_slider_value(const Target &target, float value, bool queue_change);
void update_slider_drag(const Target &target, glm::vec2 pointer);

} // namespace astralix::ui_system_core
