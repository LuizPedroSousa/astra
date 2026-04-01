#pragma once

#include "dsl/core.hpp"

namespace astralix::ui::dsl {

inline NodeSpec scroll_view() { return NodeSpec{.kind = NodeKind::ScrollView}; }

namespace detail {

inline UINodeId create_scroll_view_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_scroll_view();
}

} // namespace detail
} // namespace astralix::ui::dsl
