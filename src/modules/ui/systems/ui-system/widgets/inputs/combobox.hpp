#pragma once

#include "systems/ui-system/core.hpp"

namespace astralix::ui_system_core {

void move_combobox_highlight(const Target &target, int direction);
bool confirm_combobox_option(
    const Target &target,
    size_t index,
    const ui::UILayoutContext &context,
    bool queue_callback
);

} // namespace astralix::ui_system_core
