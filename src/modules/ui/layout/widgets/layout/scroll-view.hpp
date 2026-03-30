#pragma once

#include "layout/layout.hpp"

namespace astralix::ui {

void reset_scrollbar_geometry(UIScrollState &scroll);
void update_scrollbar_geometry(UIDocument::UINode &node);

void append_scrollbar_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
);

std::optional<UIHitResult>
hit_test_scroll_view(const UIDocument::UINode &node, UINodeId node_id, glm::vec2 point);

} // namespace astralix::ui
