#pragma once

#include "layout/layout.hpp"

namespace astralix::ui {

glm::vec2 measure_image_size(const UIDocument::UINode &node);

void append_image_node_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
);

} // namespace astralix::ui
