#pragma once

#include "layout.hpp"

#include <optional>

namespace astralix::ui {

UIRect resize_handle_rect(const UIDocument::UINode &node, UIHitPart part);

std::optional<UIHitPart>
hit_test_resize_handles(const UIDocument::UINode &node, glm::vec2 point);

void append_resize_handle_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
);

} // namespace astralix::ui
