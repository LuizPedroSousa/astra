#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec segmented_control(
    std::vector<std::string> options = {},
    size_t selected_index = 0u,
    std::string name = {}
) {
  return NodeSpec{
      .kind = NodeKind::SegmentedControl,
      .name = std::move(name),
      .option_values = std::move(options),
      .selected_index_value = selected_index,
  };
}

namespace detail {

inline UINodeId create_segmented_control_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_segmented_control(
      spec.option_values,
      spec.selected_index_value.value_or(0u),
      spec.name
  );
}

} // namespace detail
} // namespace astralix::ui::dsl
