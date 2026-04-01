#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec image(
    ResourceDescriptorID texture_id = {}
) {
  return NodeSpec{
      .kind = NodeKind::Image,
      .texture_id = std::move(texture_id),
  };
}

namespace detail {

inline UINodeId create_image_node(UIDocument &document, const NodeSpec &spec) {
  return document.create_image(spec.texture_id);
}

} // namespace detail
} // namespace astralix::ui::dsl
