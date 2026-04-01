#include "layout/widgets/layout/splitter.hpp"

#include <algorithm>

namespace astralix::ui {

glm::vec2 measure_splitter_size(
    const UIDocument &document,
    const UIDocument::UINode &node
) {
  const auto *parent = document.node(node.parent);
  const bool row_parent =
      parent == nullptr || parent->style.flex_direction == FlexDirection::Row;
  const float thickness = std::max(1.0f, node.style.splitter_thickness);
  return row_parent ? glm::vec2(thickness, 0.0f)
                    : glm::vec2(0.0f, thickness);
}

std::optional<UIHitResult>
hit_test_splitter(const UIDocument::UINode &node, UINodeId node_id) {
  if (node.type != NodeType::Splitter) {
    return std::nullopt;
  }

  return UIHitResult{.node_id = node_id, .part = UIHitPart::SplitterBar};
}

} // namespace astralix::ui
