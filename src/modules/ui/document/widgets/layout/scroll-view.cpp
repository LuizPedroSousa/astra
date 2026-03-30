#include "document/document.hpp"

#include <utility>

namespace astralix::ui {

UINodeId UIDocument::create_scroll_view(std::string name) {
  UINodeId node_id = allocate_node(NodeType::ScrollView, std::move(name));
  mutate_style(node_id, [](UIStyle &style) {
    style.overflow = Overflow::Hidden;
    style.scroll_mode = ScrollMode::Vertical;
    style.scrollbar_visibility = ScrollbarVisibility::Auto;
  });
  return node_id;
}

} // namespace astralix::ui
