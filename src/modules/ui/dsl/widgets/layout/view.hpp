#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec view(std::string name = {}) {
  return NodeSpec{.kind = NodeKind::View, .name = std::move(name)};
}

namespace detail {

inline UINodeId create_view_node(UIDocument &document, const NodeSpec &spec) {
  return document.create_view(spec.name);
}

} // namespace detail
} // namespace astralix::ui::dsl
