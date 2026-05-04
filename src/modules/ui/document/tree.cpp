#include "document.hpp"

#include "assert.hpp"

#include <algorithm>
#include <vector>

namespace astralix::ui {

void UIDocument::set_root(UINodeId root_id) {
  ASTRA_ENSURE(!is_valid_node(root_id), "ui root node is invalid");
  m_root_id = root_id;
  m_structure_dirty = true;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::detach_from_parent(UINodeId child_id) {
  UINode *child = node(child_id);
  if (child == nullptr || child->parent == k_invalid_node_id) {
    return;
  }

  UINode *parent = node(child->parent);
  if (parent != nullptr) {
    auto &children = parent->children;
    children.erase(
        std::remove(children.begin(), children.end(), child_id),
        children.end()
    );
  }

  child->parent = k_invalid_node_id;
}

void UIDocument::append_child(UINodeId parent_id, UINodeId child_id) {
  UINode *parent = node(parent_id);
  UINode *child = node(child_id);

  ASTRA_ENSURE(
      parent == nullptr || child == nullptr, "cannot append invalid ui child"
  );
  ASTRA_ENSURE(parent_id == child_id, "ui node cannot be its own parent");

  detach_from_parent(child_id);
  parent->children.push_back(child_id);
  child->parent = parent_id;

  m_structure_dirty = true;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::remove_child(UINodeId child_id) {
  if (!is_valid_node(child_id)) {
    return;
  }

  detach_from_parent(child_id);
  m_structure_dirty = true;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::clear_children(UINodeId parent_id) {
  UINode *parent = node(parent_id);
  if (parent == nullptr) {
    return;
  }

  for (UINodeId child_id : parent->children) {
    if (UINode *child = node(child_id); child != nullptr) {
      child->parent = k_invalid_node_id;
    }
  }

  parent->children.clear();
  m_structure_dirty = true;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::destroy_subtree(UINodeId node_id) {
  if (!is_valid_node(node_id)) {
    return;
  }

  detach_from_parent(node_id);

  std::vector<UINodeId> stack{node_id};
  while (!stack.empty()) {
    const UINodeId current_id = stack.back();
    stack.pop_back();

    if (!is_valid_node(current_id)) {
      continue;
    }

    UINode &current = m_nodes[current_id].node;
    for (const auto child_id : current.children) {
      stack.push_back(child_id);
    }

    if (m_root_id == current_id) {
      m_root_id = k_invalid_node_id;
    }
    if (m_hot_node == current_id) {
      m_hot_node = k_invalid_node_id;
    }
    if (m_active_node == current_id) {
      m_active_node = k_invalid_node_id;
    }
    if (m_focused_node == current_id) {
      m_focused_node = k_invalid_node_id;
    }
    if (m_open_popup_node == current_id) {
      m_open_popup_node = k_invalid_node_id;
    }
    if (m_requested_focus_node == current_id) {
      m_requested_focus_node = k_invalid_node_id;
    }
    m_pointer_capture_requests.erase(
        std::remove_if(
            m_pointer_capture_requests.begin(),
            m_pointer_capture_requests.end(),
            [current_id](const UIPointerCaptureRequest &request) {
              return request.node_id == current_id;
            }
        ),
        m_pointer_capture_requests.end()
    );

    m_open_popover_stack.erase(
        std::remove(
            m_open_popover_stack.begin(),
            m_open_popover_stack.end(),
            current_id
        ),
        m_open_popover_stack.end()
    );

    current.children.clear();
    current.parent = k_invalid_node_id;
    m_nodes[current_id].alive = false;
  }

  m_structure_dirty = true;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

} // namespace astralix::ui
