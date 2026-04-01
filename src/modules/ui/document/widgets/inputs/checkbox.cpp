#include "document/document.hpp"

#include <utility>

namespace astralix::ui {

UINodeId UIDocument::create_checkbox(
    std::string label,
    bool checked
) {
  UINodeId node_id = allocate_node(NodeType::Checkbox);
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->focusable = true;
    node->text = std::move(label);
    node->checkbox.checked = checked;
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.padding = UIEdges::symmetric(10.0f, 8.0f);
    style.border_radius = 10.0f;
    style.cursor = CursorStyle::Pointer;
    style.text_color = glm::vec4(0.92f, 0.96f, 1.0f, 1.0f);
    style.hovered_style.background_color =
        glm::vec4(0.08f, 0.14f, 0.22f, 0.55f);
    style.pressed_style.background_color =
        glm::vec4(0.1f, 0.18f, 0.28f, 0.75f);
    style.focused_style.border_color = glm::vec4(0.78f, 0.88f, 1.0f, 0.92f);
    style.focused_style.border_width = 1.0f;
    style.disabled_style.opacity = 0.55f;
  });

  return node_id;
}

void UIDocument::set_checked(UINodeId node_id, bool checked) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Checkbox ||
      target->checkbox.checked == checked) {
    return;
  }

  target->checkbox.checked = checked;
  m_paint_dirty = true;
}

bool UIDocument::checked(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Checkbox) {
    return false;
  }

  return target->checkbox.checked;
}

} // namespace astralix::ui
