#pragma once

#include "dsl/core.hpp"

namespace astralix::ui::dsl {

inline NodeSpec popover() { return NodeSpec{.kind = NodeKind::Popover}; }

namespace detail {

inline UINodeId create_popover_node(UIDocument &document, const NodeSpec &spec) {
  return document.create_popover();
}

} // namespace detail
} // namespace astralix::ui::dsl
