#include "document/document.hpp"

#include <utility>

namespace astralix::ui {

UINodeId UIDocument::create_splitter(std::string name) {
  UINodeId node_id = allocate_node(NodeType::Splitter, std::move(name));
  mutate_style(node_id, [](UIStyle &style) {
    style.align_self = AlignSelf::Stretch;
    style.flex_shrink = 0.0f;
    style.background_color = glm::vec4(0.2f, 0.31f, 0.44f, 0.38f);
    style.hovered_style.background_color =
        glm::vec4(0.42f, 0.62f, 0.84f, 0.72f);
    style.pressed_style.background_color =
        glm::vec4(0.66f, 0.82f, 0.98f, 0.9f);
  });
  return node_id;
}

} // namespace astralix::ui
