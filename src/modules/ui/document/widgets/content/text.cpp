#include "document/document.hpp"

#include <utility>

namespace astralix::ui {

UINodeId UIDocument::create_text(std::string text, std::string name) {
  UINodeId node_id = allocate_node(NodeType::Text, std::move(name));
  set_text(node_id, std::move(text));
  return node_id;
}

} // namespace astralix::ui
