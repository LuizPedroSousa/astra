#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec text_input(
    std::string value = {},
    std::string placeholder = {},
    std::string name = {}
) {
  return NodeSpec{
      .kind = NodeKind::TextInput,
      .name = std::move(name),
      .text = std::move(value),
      .placeholder = std::move(placeholder),
  };
}

inline NodeSpec input(
    std::string value = {},
    std::string placeholder = {},
    std::string name = {}
) {
  return text_input(
      std::move(value),
      std::move(placeholder),
      std::move(name)
  );
}

namespace detail {

inline UINodeId create_text_input_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_text_input(spec.text, spec.placeholder, spec.name);
}

} // namespace detail
} // namespace astralix::ui::dsl
