#include "document.hpp"

#include <vector>

namespace astralix::ui {

Ref<UIDocument> UIDocument::create() { return create_ref<UIDocument>(); }

UINodeId UIDocument::allocate_node(NodeType type, std::string name) {
  if (m_nodes.empty()) {
    m_nodes.push_back(NodeSlot{});
  }

  UINodeId node_id = static_cast<UINodeId>(m_nodes.size());
  m_nodes.push_back(NodeSlot{
      .node =
          UINode{
              .id = node_id,
              .type = type,
              .name = std::move(name),
          },
      .alive = true,
  });

  m_structure_dirty = true;
  m_layout_dirty = true;
  m_paint_dirty = true;
  return node_id;
}

bool UIDocument::is_valid_node(UINodeId node_id) const {
  return node_id != k_invalid_node_id && node_id < m_nodes.size() &&
         m_nodes[node_id].alive;
}

UIDocument::UINode *UIDocument::node(UINodeId node_id) {
  return is_valid_node(node_id) ? &m_nodes[node_id].node : nullptr;
}

const UIDocument::UINode *UIDocument::node(UINodeId node_id) const {
  return is_valid_node(node_id) ? &m_nodes[node_id].node : nullptr;
}

void UIDocument::set_canvas_size(glm::vec2 canvas_size) {
  if (m_canvas_size == canvas_size) {
    return;
  }

  m_canvas_size = canvas_size;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_root_font_size(float root_font_size) {
  if (m_root_font_size == root_font_size) {
    return;
  }

  m_root_font_size = root_font_size;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::mark_layout_dirty() {
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::mark_paint_dirty() { m_paint_dirty = true; }

void UIDocument::clear_layout_dirty() {
  m_structure_dirty = false;
  m_layout_dirty = false;
}

void UIDocument::clear_paint_dirty() { m_paint_dirty = false; }

void UIDocument::clear_dirty() {
  m_structure_dirty = false;
  m_layout_dirty = false;
  m_paint_dirty = false;
}

UINodeId UIDocument::parent(UINodeId node_id) const {
  const UINode *target = node(node_id);
  return target != nullptr ? target->parent : k_invalid_node_id;
}

UIScrollState *UIDocument::scroll_state(UINodeId node_id) {
  UINode *target = node(node_id);
  return target != nullptr ? &target->layout.scroll : nullptr;
}

const UIScrollState *UIDocument::scroll_state(UINodeId node_id) const {
  const UINode *target = node(node_id);
  return target != nullptr ? &target->layout.scroll : nullptr;
}

std::vector<UINodeId> UIDocument::root_to_leaf_order() const {
  std::vector<UINodeId> order;
  if (!is_valid_node(m_root_id)) {
    return order;
  }

  std::vector<UINodeId> stack{m_root_id};
  while (!stack.empty()) {
    const UINodeId node_id = stack.back();
    stack.pop_back();
    order.push_back(node_id);

    const UINode *current = node(node_id);
    if (current == nullptr) {
      continue;
    }

    for (auto it = current->children.rbegin(); it != current->children.rend();
         ++it) {
      stack.push_back(*it);
    }
  }

  return order;
}

} // namespace astralix::ui
