#include "document/document.hpp"

#include <utility>

namespace astralix::ui {

UINodeId UIDocument::create_image(ResourceDescriptorID texture_id) {
  UINodeId node_id = allocate_node(NodeType::Image);
  set_texture(node_id, std::move(texture_id));
  return node_id;
}

} // namespace astralix::ui
