#pragma once

#include "layout/layout.hpp"

namespace astralix::ui {

glm::vec2 measure_text_size(
    const UIDocument::UINode &node,
    const UILayoutContext &context
);

void append_text_node_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved
);

} // namespace astralix::ui
