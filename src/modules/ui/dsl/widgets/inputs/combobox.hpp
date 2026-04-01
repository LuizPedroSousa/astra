#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec combobox(
    std::string value = {},
    std::string placeholder = {}
) {
  return NodeSpec{
      .kind = NodeKind::Combobox,
      .text = std::move(value),
      .placeholder = std::move(placeholder),
  };
}

namespace detail {

inline UINodeId create_combobox_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_combobox(spec.text, spec.placeholder);
}

} // namespace detail
} // namespace astralix::ui::dsl
