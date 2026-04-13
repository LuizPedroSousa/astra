#pragma once

#include "dsl/core.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec render_image_view(
    RenderImageExportKey render_image_key
) {
  return NodeSpec{
      .kind = NodeKind::RenderImageView,
      .render_image_key = render_image_key,
  };
}

inline NodeSpec render_image_view(
    RenderImageResource resource,
    RenderImageAspect aspect
) {
  return render_image_view(make_render_image_export_key(resource, aspect));
}

inline NodeSpec render_image_view(GBufferAspect aspect) {
  return render_image_view(make_g_buffer_export_key(aspect));
}

namespace detail {

inline UINodeId create_render_image_view_node(
    UIDocument &document,
    const NodeSpec &spec
) {
  return document.create_render_image_view(
      spec.render_image_key.value_or(RenderImageExportKey{})
  );
}

} // namespace detail
} // namespace astralix::ui::dsl
