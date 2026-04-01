#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec text(std::string value = {}) {
  return NodeSpec{
      .kind = NodeKind::Text,
      .text = std::move(value),
  };
}

namespace detail {

inline UINodeId create_text_node(UIDocument &document, const NodeSpec &spec) {
  return document.create_text(spec.text);
}

} // namespace detail
} // namespace astralix::ui::dsl
