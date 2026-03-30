#include "document.hpp"

namespace astralix::ui {
namespace {

bool node_has_popup(const UIDocument::UINode &node) {
  return node.type == NodeType::Select || node.type == NodeType::Combobox;
}

void close_popup_state(UIDocument::UINode &node) {
  if (node.type == NodeType::Select) {
    node.select.open = false;
  } else if (node.type == NodeType::Combobox) {
    node.combobox.open = false;
  }
}

} // namespace

void UIDocument::set_hot_node(UINodeId node_id) {
  if (m_hot_node == node_id) {
    return;
  }

  if (UINode *previous = node(m_hot_node); previous != nullptr) {
    previous->paint_state.hovered = false;
  }

  m_hot_node = node_id;

  if (UINode *current = node(m_hot_node); current != nullptr) {
    current->paint_state.hovered = true;
  }

  m_paint_dirty = true;
}

void UIDocument::set_active_node(UINodeId node_id) {
  if (m_active_node == node_id) {
    return;
  }

  if (UINode *previous = node(m_active_node); previous != nullptr) {
    previous->paint_state.pressed = false;
  }

  m_active_node = node_id;

  if (UINode *current = node(m_active_node); current != nullptr) {
    current->paint_state.pressed = true;
  }

  m_paint_dirty = true;
}

void UIDocument::set_focused_node(UINodeId node_id) {
  if (m_focused_node == node_id) {
    return;
  }

  if (m_open_popup_node != k_invalid_node_id && m_open_popup_node != node_id) {
    if (UINode *previous = node(m_open_popup_node); previous != nullptr &&
                                                    node_has_popup(*previous)) {
      if (previous->type == NodeType::Select) {
        set_select_open(m_open_popup_node, false);
      } else if (previous->type == NodeType::Combobox) {
        set_combobox_open(m_open_popup_node, false);
      }
    }
  }

  if (UINode *previous = node(m_focused_node); previous != nullptr) {
    previous->paint_state.focused = false;
  }

  m_focused_node = node_id;

  if (UINode *current = node(m_focused_node); current != nullptr) {
    current->paint_state.focused = true;
  }

  m_paint_dirty = true;
}

void UIDocument::clear_focus() { set_focused_node(k_invalid_node_id); }

void UIDocument::request_focus(UINodeId node_id) {
  if (node_id != k_invalid_node_id && !is_valid_node(node_id)) {
    return;
  }

  m_requested_focus_node = node_id;
}

UINodeId UIDocument::consume_requested_focus() {
  const UINodeId requested = m_requested_focus_node;
  m_requested_focus_node = k_invalid_node_id;
  return requested;
}

void UIDocument::suppress_next_character_input(uint32_t codepoint) {
  m_suppressed_character_input_codepoint = codepoint;
}

bool UIDocument::consume_suppressed_character_input(uint32_t codepoint) {
  if (!m_suppressed_character_input_codepoint.has_value() ||
      *m_suppressed_character_input_codepoint != codepoint) {
    return false;
  }

  m_suppressed_character_input_codepoint.reset();
  return true;
}

UINodeId UIDocument::open_select_node() const {
  const UINode *target = node(m_open_popup_node);
  return target != nullptr && target->type == NodeType::Select
             ? m_open_popup_node
             : k_invalid_node_id;
}

UINodeId UIDocument::open_combobox_node() const {
  const UINode *target = node(m_open_popup_node);
  return target != nullptr && target->type == NodeType::Combobox
             ? m_open_popup_node
             : k_invalid_node_id;
}

} // namespace astralix::ui
