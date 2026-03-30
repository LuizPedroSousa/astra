#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec button(
    std::string label,
    std::function<void()> on_click = {},
    std::string name = {}
) {
  return NodeSpec{
      .kind = NodeKind::Button,
      .name = std::move(name),
      .text = std::move(label),
      .on_click_callback = std::move(on_click),
  };
}

namespace detail {

inline UINodeId create_button_node(UIDocument &document, const NodeSpec &spec) {
  return document.create_button(spec.text, spec.on_click_callback, spec.name);
}

} // namespace detail
} // namespace astralix::ui::dsl
