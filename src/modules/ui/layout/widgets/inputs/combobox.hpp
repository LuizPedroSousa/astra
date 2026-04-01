#pragma once

#include "layout/layout.hpp"

namespace astralix::ui {

glm::vec2 measure_combobox_size(
    const UIDocument::UINode &node,
    const UILayoutContext &context
);

void update_combobox_layout(
    UIDocument::UINode &node,
    const UILayoutContext &context
);

void append_combobox_field_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved
);

void append_combobox_overlay_commands(
    UIDocument &document,
    UINodeId node_id,
    const UILayoutContext &context
);

} // namespace astralix::ui
