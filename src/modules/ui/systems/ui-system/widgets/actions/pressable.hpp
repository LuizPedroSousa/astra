#pragma once

#include "systems/ui-system/core.hpp"

namespace astralix::ui_system_core {

void queue_press_callback(const Target &target);
void queue_release_callback(const Target &target);
void queue_click_callback(const Target &target);

} // namespace astralix::ui_system_core
