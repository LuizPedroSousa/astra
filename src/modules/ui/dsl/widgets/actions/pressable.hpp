#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec pressable(std::string name = {}) {
  return NodeSpec{.kind = NodeKind::Pressable, .name = std::move(name)};
}

namespace detail {

inline UINodeId create_pressable_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_pressable(spec.name);
}

} // namespace detail
} // namespace astralix::ui::dsl
