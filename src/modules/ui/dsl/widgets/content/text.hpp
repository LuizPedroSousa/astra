#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec text(std::string value = {}, std::string name = {}) {
  return NodeSpec{
      .kind = NodeKind::Text,
      .name = std::move(name),
      .text = std::move(value),
  };
}

namespace detail {

inline UINodeId create_text_node(UIDocument &document, const NodeSpec &spec) {
  return document.create_text(spec.text, spec.name);
}

} // namespace detail
} // namespace astralix::ui::dsl
