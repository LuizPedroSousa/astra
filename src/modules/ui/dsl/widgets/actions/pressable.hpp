#pragma once

#include "dsl/core.hpp"

namespace astralix::ui::dsl {

inline NodeSpec pressable() { return NodeSpec{.kind = NodeKind::Pressable}; }

namespace detail {

inline UINodeId create_pressable_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_pressable();
}

} // namespace detail
} // namespace astralix::ui::dsl
