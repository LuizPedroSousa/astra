#include "document/document.hpp"

#include <utility>

namespace astralix::ui {

UINodeId UIDocument::create_text(std::string text) {
  UINodeId node_id = allocate_node(NodeType::Text);
  set_text(node_id, std::move(text));
  return node_id;
}

} // namespace astralix::ui
