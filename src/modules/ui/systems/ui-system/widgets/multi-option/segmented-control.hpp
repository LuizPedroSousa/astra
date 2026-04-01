#pragma once

#include "systems/ui-system/core.hpp"

namespace astralix::ui_system_core {

bool select_segmented_option(const Target &target, size_t index, bool queue_callback);
bool move_segmented_selection(const Target &target, int direction, bool queue_callback);

} // namespace astralix::ui_system_core
