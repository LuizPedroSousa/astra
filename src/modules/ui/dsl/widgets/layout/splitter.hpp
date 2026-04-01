#pragma once

#include "dsl/core.hpp"

namespace astralix::ui::dsl {

inline NodeSpec splitter() { return NodeSpec{.kind = NodeKind::Splitter}; }

namespace detail {

inline UINodeId create_splitter_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_splitter();
}

} // namespace detail
} // namespace astralix::ui::dsl
