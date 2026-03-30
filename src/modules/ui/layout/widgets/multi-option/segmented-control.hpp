#pragma once

#include "layout/layout.hpp"

namespace astralix::ui {

glm::vec2 measure_segmented_control_size(
    const UIDocument::UINode &node,
    const UILayoutContext &context
);

void update_segmented_control_layout(
    UIDocument::UINode &node,
    const UILayoutContext &context
);

void append_segmented_control_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved
);

} // namespace astralix::ui
