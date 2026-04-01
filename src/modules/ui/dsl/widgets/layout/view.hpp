#pragma once

#include "dsl/core.hpp"

namespace astralix::ui::dsl {

inline NodeSpec view() { return NodeSpec{.kind = NodeKind::View}; }

namespace detail {

inline UINodeId create_view_node(UIDocument &document, const NodeSpec &spec) {
  return document.create_view();
}

} // namespace detail
} // namespace astralix::ui::dsl
