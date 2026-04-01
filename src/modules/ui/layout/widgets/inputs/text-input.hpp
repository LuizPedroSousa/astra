#pragma once

#include "layout/layout.hpp"

namespace astralix::ui {

glm::vec2 measure_text_input_size(
    const UIDocument::UINode &node,
    const UILayoutContext &context
);

void append_editable_text_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved,
    const UIRect &text_rect
);

} // namespace astralix::ui
