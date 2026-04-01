#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec checkbox(
    std::string label = {},
    bool checked = false
) {
  return NodeSpec{
      .kind = NodeKind::Checkbox,
      .text = std::move(label),
      .checked_value = checked,
  };
}

namespace detail {

inline UINodeId create_checkbox_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_checkbox(
      spec.text,
      spec.checked_value.value_or(false)
  );
}

} // namespace detail
} // namespace astralix::ui::dsl
