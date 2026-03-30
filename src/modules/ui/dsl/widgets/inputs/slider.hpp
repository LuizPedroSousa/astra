#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec slider(
    float value = 0.0f,
    float min_value = 0.0f,
    float max_value = 1.0f,
    std::string name = {}
) {
  return NodeSpec{
      .kind = NodeKind::Slider,
      .name = std::move(name),
      .slider_value = value,
      .slider_min_value = min_value,
      .slider_max_value = max_value,
  };
}

namespace detail {

inline UINodeId create_slider_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_slider(
      spec.slider_value.value_or(0.0f),
      spec.slider_min_value.value_or(0.0f),
      spec.slider_max_value.value_or(1.0f),
      spec.slider_step_value.value_or(0.1f),
      spec.name
  );
}

} // namespace detail
} // namespace astralix::ui::dsl
