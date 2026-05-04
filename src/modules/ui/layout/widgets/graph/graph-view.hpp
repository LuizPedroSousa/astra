#pragma once

#include "layout/layout.hpp"

namespace astralix::ui {

glm::vec2 measure_graph_view_size(
    const UIDocument::UINode &node,
    const UILayoutContext &context
);

void update_graph_view_layout(
    UIDocument::UINode &node,
    const UILayoutContext &context
);

void append_graph_view_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved
);

std::optional<UICustomHitData> hit_test_graph_view(
    const UIDocument::UINode &node,
    glm::vec2 local_position
);

} // namespace astralix::ui
