#pragma once

#include "layout/layout.hpp"

namespace astralix::ui {

glm::vec2 measure_chip_group_size(
    const UIDocument::UINode &node,
    const UILayoutContext &context
);

void update_chip_group_layout(
    UIDocument::UINode &node,
    const UILayoutContext &context
);

void append_chip_group_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved
);

} // namespace astralix::ui
