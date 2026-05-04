#pragma once

#include "dsl/core.hpp"

namespace astralix::ui::dsl {

inline NodeSpec graph_view(GraphViewSpec spec) {
  NodeSpec node;
  node.kind = NodeKind::GraphView;
  node.graph_view_spec = std::move(spec);
  return node;
}

namespace detail {

inline UINodeId create_graph_view_node(
    UIDocument &document,
    const NodeSpec &
) {
  return document.create_graph_view();
}

} // namespace detail
} // namespace astralix::ui::dsl
