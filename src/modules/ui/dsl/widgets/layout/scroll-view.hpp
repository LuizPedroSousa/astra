#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec scroll_view(std::string name = {}) {
  return NodeSpec{.kind = NodeKind::ScrollView, .name = std::move(name)};
}

namespace detail {

inline UINodeId create_scroll_view_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_scroll_view(spec.name);
}

} // namespace detail
} // namespace astralix::ui::dsl
