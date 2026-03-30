#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec image(
    ResourceDescriptorID texture_id = {},
    std::string name = {}
) {
  return NodeSpec{
      .kind = NodeKind::Image,
      .name = std::move(name),
      .texture_id = std::move(texture_id),
  };
}

namespace detail {

inline UINodeId create_image_node(UIDocument &document, const NodeSpec &spec) {
  return document.create_image(spec.texture_id, spec.name);
}

} // namespace detail
} // namespace astralix::ui::dsl
