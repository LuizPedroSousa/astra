#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec text_input(
    std::string value = {},
    std::string placeholder = {}
) {
  return NodeSpec{
      .kind = NodeKind::TextInput,
      .text = std::move(value),
      .placeholder = std::move(placeholder),
  };
}

inline NodeSpec input(
    std::string value = {},
    std::string placeholder = {}
) {
  return text_input(std::move(value), std::move(placeholder));
}

namespace detail {

inline UINodeId create_text_input_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_text_input(spec.text, spec.placeholder);
}

} // namespace detail
} // namespace astralix::ui::dsl
