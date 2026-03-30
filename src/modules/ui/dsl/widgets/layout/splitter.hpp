#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec splitter(std::string name = {}) {
  return NodeSpec{.kind = NodeKind::Splitter, .name = std::move(name)};
}

namespace detail {

inline UINodeId create_splitter_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_splitter(spec.name);
}

} // namespace detail
} // namespace astralix::ui::dsl
