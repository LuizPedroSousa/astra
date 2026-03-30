#pragma once

#include "layout/layout.hpp"

namespace astralix::ui {

glm::vec2
measure_slider_size(const UIDocument::UINode &node, const UILayoutContext &context);

void update_slider_layout(UIDocument::UINode &node);

void append_slider_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
);

} // namespace astralix::ui
