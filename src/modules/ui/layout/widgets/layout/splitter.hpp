#pragma once

#include "layout/layout.hpp"

namespace astralix::ui {

glm::vec2 measure_splitter_size(
    const UIDocument &document,
    const UIDocument::UINode &node
);

std::optional<UIHitResult>
hit_test_splitter(const UIDocument::UINode &node, UINodeId node_id);

} // namespace astralix::ui
