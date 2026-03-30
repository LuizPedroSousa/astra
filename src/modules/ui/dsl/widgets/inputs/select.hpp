#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec select(
    std::vector<std::string> options = {},
    size_t selected_index = 0u,
    std::string placeholder = {},
    std::string name = {}
) {
  return NodeSpec{
      .kind = NodeKind::Select,
      .name = std::move(name),
      .placeholder = std::move(placeholder),
      .option_values = std::move(options),
      .selected_index_value = selected_index,
  };
}

namespace detail {

inline UINodeId create_select_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_select(
      spec.option_values,
      spec.selected_index_value.value_or(0u),
      spec.placeholder,
      spec.name
  );
}

} // namespace detail
} // namespace astralix::ui::dsl
