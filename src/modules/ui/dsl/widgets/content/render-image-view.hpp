#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec render_image_view(
    RenderImageExportKey render_image_key,
    std::string name = {}
) {
  return NodeSpec{
      .kind = NodeKind::RenderImageView,
      .name = std::move(name),
      .render_image_key = render_image_key,
  };
}

inline NodeSpec render_image_view(
    RenderImageResource resource,
    RenderImageAspect aspect,
    std::string name = {}
) {
  return render_image_view(
      RenderImageExportKey{.resource = resource, .aspect = aspect},
      std::move(name)
  );
}

namespace detail {

inline UINodeId create_render_image_view_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_render_image_view(
      spec.render_image_key.value_or(RenderImageExportKey{}),
      spec.name
  );
}

} // namespace detail
} // namespace astralix::ui::dsl
