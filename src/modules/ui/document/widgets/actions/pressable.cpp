#include "document/document.hpp"
#include "types.hpp"

#include <utility>

namespace astralix::ui {

UINodeId UIDocument::create_pressable(std::string name) {
  UINodeId node_id = allocate_node(NodeType::Pressable, std::move(name));
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->focusable = true;
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.cursor = CursorStyle::Pointer;
  });

  return node_id;
}

} // namespace astralix::ui
