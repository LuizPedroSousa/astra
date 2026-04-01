#include "document/document.hpp"

#include <algorithm>
#include <utility>

namespace astralix::ui {
namespace {

void normalize_select_state(UISelectState &select) {
  if (select.options.empty()) {
    select.selected_index = 0u;
    select.highlighted_index = 0u;
    select.open = false;
    return;
  }

  const size_t max_index = select.options.size() - 1u;
  select.selected_index = std::min(select.selected_index, max_index);
  select.highlighted_index = std::min(select.highlighted_index, max_index);
}

void close_popup_state(UIDocument::UINode &node) {
  if (node.type == NodeType::Select) {
    node.select.open = false;
  } else if (node.type == NodeType::Combobox) {
    node.combobox.open = false;
  }
}

} // namespace

UINodeId UIDocument::create_select(
    std::vector<std::string> options,
    size_t selected_index,
    std::string placeholder
) {
  UINodeId node_id = allocate_node(NodeType::Select);
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->focusable = true;
    node->placeholder = std::move(placeholder);
    node->select.options = std::move(options);
    node->select.selected_index = selected_index;
    node->select.highlighted_index = selected_index;
    normalize_select_state(node->select);
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.padding = UIEdges::symmetric(12.0f, 10.0f);
    style.border_radius = 10.0f;
    style.border_width = 1.0f;
    style.cursor = CursorStyle::Pointer;
    style.background_color = glm::vec4(0.04f, 0.08f, 0.13f, 0.9f);
    style.border_color = glm::vec4(0.36f, 0.48f, 0.61f, 0.45f);
    style.text_color = glm::vec4(0.94f, 0.97f, 1.0f, 1.0f);
    style.placeholder_text_color = glm::vec4(0.59f, 0.68f, 0.78f, 0.88f);
    style.hovered_style.border_color = glm::vec4(0.5f, 0.63f, 0.8f, 0.65f);
    style.focused_style.border_color = glm::vec4(0.74f, 0.84f, 0.98f, 0.95f);
    style.focused_style.border_width = 2.0f;
    style.disabled_style.opacity = 0.6f;
  });

  return node_id;
}

void UIDocument::set_select_options(
    UINodeId node_id,
    std::vector<std::string> options
) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Select) {
    return;
  }

  target->select.options = std::move(options);
  normalize_select_state(target->select);
  if (target->select.options.empty()) {
    set_select_open(node_id, false);
  } else {
    m_layout_dirty = true;
    m_paint_dirty = true;
  }
}

const std::vector<std::string> *
UIDocument::select_options(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Select) {
    return nullptr;
  }

  return &target->select.options;
}

void UIDocument::set_selected_index(UINodeId node_id, size_t selected_index) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Select) {
    return;
  }

  const size_t previous_selected = target->select.selected_index;
  target->select.selected_index = selected_index;
  target->select.highlighted_index = selected_index;
  normalize_select_state(target->select);

  if (target->select.selected_index == previous_selected) {
    return;
  }

  m_layout_dirty = true;
  m_paint_dirty = true;
}

size_t UIDocument::selected_index(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Select) {
    return 0u;
  }

  return target->select.selected_index;
}

void UIDocument::set_select_open(UINodeId node_id, bool open) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Select) {
    return;
  }

  if (open && (!target->visible || !target->enabled ||
               target->select.options.empty())) {
    open = false;
  }

  if (open && !m_open_popover_stack.empty()) {
    close_all_popovers();
  }

  if (open && m_open_popup_node != k_invalid_node_id &&
      m_open_popup_node != node_id) {
    if (UINode *previous = node(m_open_popup_node); previous != nullptr) {
      close_popup_state(*previous);
    }
    m_open_popup_node = k_invalid_node_id;
    m_layout_dirty = true;
    m_paint_dirty = true;
  }

  if (target->select.open == open &&
      (!open || m_open_popup_node == node_id)) {
    return;
  }

  target->select.open = open;
  if (open) {
    target->select.highlighted_index = target->select.selected_index;
    normalize_select_state(target->select);
    m_open_popup_node = node_id;
  } else if (m_open_popup_node == node_id) {
    m_open_popup_node = k_invalid_node_id;
  }

  m_layout_dirty = true;
  m_paint_dirty = true;
}

bool UIDocument::select_open(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Select) {
    return false;
  }

  return target->select.open;
}

} // namespace astralix::ui
