#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec icon_button(
    ResourceDescriptorID texture_id = {},
    std::function<void()> on_click = {},
    std::string name = {}
) {
  return NodeSpec{
      .kind = NodeKind::IconButton,
      .name = std::move(name),
      .texture_id = std::move(texture_id),
      .on_click_callback = std::move(on_click),
  };
}

namespace detail {

inline UINodeId create_icon_button_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_icon_button(
      spec.texture_id,
      spec.on_click_callback,
      spec.name
  );
}

} // namespace detail
} // namespace astralix::ui::dsl
