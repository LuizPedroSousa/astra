#include "document/document.hpp"

#include <algorithm>
#include <utility>

namespace astralix::ui {
namespace {

void close_legacy_popup(UIDocument &document) {
  const UINodeId popup_id = document.open_popup_node();
  if (popup_id == k_invalid_node_id) {
    return;
  }

  if (auto *previous = document.node(popup_id); previous != nullptr) {
    if (previous->type == NodeType::Select) {
      previous->select.open = false;
    } else if (previous->type == NodeType::Combobox) {
      previous->combobox.open = false;
    }
  }
}

} // namespace

UINodeId UIDocument::create_popover() {
  const UINodeId node_id = allocate_node(NodeType::Popover);
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->visible = false;
    node->enabled = true;
    node->focusable = false;
    node->style.position_type = PositionType::Absolute;
  }

  return node_id;
}

void UIDocument::open_popover_at(
    UINodeId node_id,
    glm::vec2 anchor_point,
    UIPopupPlacement placement,
    size_t depth
) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Popover ||
      !target->enabled) {
    return;
  }

  close_popovers_from_depth(depth);
  if (m_open_popup_node != k_invalid_node_id) {
    close_legacy_popup(*this);
    m_open_popup_node = k_invalid_node_id;
  }

  target->popover.open = true;
  target->popover.anchor_kind = UIPopupAnchorKind::Cursor;
  target->popover.placement = placement;
  target->popover.anchor_node_id = k_invalid_node_id;
  target->popover.anchor_point = anchor_point;
  target->popover.depth = depth;
  target->visible = true;

  m_open_popover_stack.erase(
      std::remove(
          m_open_popover_stack.begin(),
          m_open_popover_stack.end(),
          node_id
      ),
      m_open_popover_stack.end()
  );
  m_open_popover_stack.push_back(node_id);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::open_popover_anchored_to(
    UINodeId node_id,
    UINodeId anchor_node_id,
    UIPopupPlacement placement,
    size_t depth
) {
  if (node(anchor_node_id) == nullptr) {
    close_popover(node_id);
    return;
  }

  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Popover ||
      !target->enabled) {
    return;
  }

  close_popovers_from_depth(depth);
  if (m_open_popup_node != k_invalid_node_id) {
    close_legacy_popup(*this);
    m_open_popup_node = k_invalid_node_id;
  }

  target->popover.open = true;
  target->popover.anchor_kind = UIPopupAnchorKind::Node;
  target->popover.placement = placement;
  target->popover.anchor_node_id = anchor_node_id;
  target->popover.anchor_point = glm::vec2(0.0f);
  target->popover.depth = depth;
  target->visible = true;

  m_open_popover_stack.erase(
      std::remove(
          m_open_popover_stack.begin(),
          m_open_popover_stack.end(),
          node_id
      ),
      m_open_popover_stack.end()
  );
  m_open_popover_stack.push_back(node_id);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::close_popover(UINodeId node_id) {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Popover ||
      !target->popover.open) {
    return;
  }

  close_popovers_from_depth(target->popover.depth);
}

void UIDocument::close_popovers_from_depth(size_t depth) {
  bool changed = false;

  for (auto it = m_open_popover_stack.begin(); it != m_open_popover_stack.end();) {
    UINode *target = node(*it);
    if (target == nullptr || target->type != NodeType::Popover) {
      it = m_open_popover_stack.erase(it);
      changed = true;
      continue;
    }

    if (target->popover.open && target->popover.depth >= depth) {
      target->popover.open = false;
      target->visible = false;
      it = m_open_popover_stack.erase(it);
      changed = true;
      continue;
    }

    ++it;
  }

  if (changed) {
    m_layout_dirty = true;
    m_paint_dirty = true;
  }
}

void UIDocument::close_all_popovers() { close_popovers_from_depth(0u); }

} // namespace astralix::ui
