#pragma once

#include "layout/layout.hpp"

namespace astralix::ui {

void append_line_chart_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
);

} // namespace astralix::ui
