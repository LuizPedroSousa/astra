#include "document/document.hpp"

#include <utility>

namespace astralix::ui {

UINodeId UIDocument::create_render_image_view(
    RenderImageExportKey render_image_key
) {
  UINodeId node_id = allocate_node(NodeType::RenderImageView);
  set_render_image_key(node_id, render_image_key);
  return node_id;
}

} // namespace astralix::ui
