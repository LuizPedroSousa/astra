#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec chip_group(
    std::vector<std::string> options = {},
    std::vector<bool> selected = {},
    std::string name = {}
) {
  return NodeSpec{
      .kind = NodeKind::ChipGroup,
      .name = std::move(name),
      .option_values = std::move(options),
      .chip_selected_values = std::move(selected),
  };
}

namespace detail {

inline UINodeId create_chip_group_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_chip_group(
      spec.option_values,
      spec.chip_selected_values,
      spec.name
  );
}

} // namespace detail
} // namespace astralix::ui::dsl
