#include "document/document.hpp"

#include <algorithm>
#include <utility>

namespace astralix::ui {
namespace {

void normalize_combobox_state(UIComboboxState &combobox) {
  if (combobox.options.empty()) {
    combobox.highlighted_index = 0u;
    combobox.open = false;
    return;
  }

  combobox.highlighted_index =
      std::min(combobox.highlighted_index, combobox.options.size() - 1u);
}

void close_popup_state(UIDocument::UINode &node) {
  if (node.type == NodeType::Select) {
    node.select.open = false;
  } else if (node.type == NodeType::Combobox) {
    node.combobox.open = false;
  }
}

} // namespace

UINodeId UIDocument::create_combobox(
    std::string value,
    std::string placeholder
) {
  UINodeId node_id = allocate_node(NodeType::Combobox);
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->focusable = true;
    node->text = std::move(value);
    node->placeholder = std::move(placeholder);
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.padding = UIEdges::symmetric(12.0f, 10.0f);
    style.border_radius = 10.0f;
    style.border_width = 1.0f;
    style.overflow = Overflow::Hidden;
    style.cursor = CursorStyle::Default;
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

void UIDocument::set_combobox_options(
    UINodeId node_id,
    std::vector<std::string> options
) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Combobox) {
    return;
  }

  target->combobox.options = std::move(options);
  normalize_combobox_state(target->combobox);
  if (target->combobox.options.empty()) {
    set_combobox_open(node_id, false);
  } else {
    m_layout_dirty = true;
    m_paint_dirty = true;
  }
}

const std::vector<std::string> *
UIDocument::combobox_options(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Combobox) {
    return nullptr;
  }

  return &target->combobox.options;
}

void UIDocument::set_combobox_open(UINodeId node_id, bool open) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Combobox) {
    return;
  }

  if (open && (!target->visible || !target->enabled ||
               target->combobox.options.empty())) {
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

  if (target->combobox.open == open &&
      (!open || m_open_popup_node == node_id)) {
    return;
  }

  target->combobox.open = open;
  if (open) {
    normalize_combobox_state(target->combobox);
    m_open_popup_node = node_id;
  } else if (m_open_popup_node == node_id) {
    m_open_popup_node = k_invalid_node_id;
  }

  m_layout_dirty = true;
  m_paint_dirty = true;
}

bool UIDocument::combobox_open(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Combobox) {
    return false;
  }

  return target->combobox.open;
}

void UIDocument::set_combobox_highlighted_index(
    UINodeId node_id,
    size_t highlighted_index
) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Combobox) {
    return;
  }

  if (target->combobox.options.empty()) {
    highlighted_index = 0u;
  } else {
    highlighted_index =
        std::min(highlighted_index, target->combobox.options.size() - 1u);
  }

  if (target->combobox.highlighted_index == highlighted_index) {
    return;
  }

  target->combobox.highlighted_index = highlighted_index;
  m_paint_dirty = true;
}

size_t UIDocument::combobox_highlighted_index(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Combobox) {
    return 0u;
  }

  return target->combobox.highlighted_index;
}

} // namespace astralix::ui
