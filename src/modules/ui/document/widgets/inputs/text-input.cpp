#include "document/document.hpp"

#include <algorithm>
#include <utility>

namespace astralix::ui {
namespace {

size_t clamp_text_index(const UIDocument::UINode &node, size_t index) {
  return std::min(index, node.text.size());
}

UITextSelection
clamp_text_selection(const UIDocument::UINode &node, UITextSelection selection) {
  selection.anchor = clamp_text_index(node, selection.anchor);
  selection.focus = clamp_text_index(node, selection.focus);
  return selection;
}

void clamp_text_runtime_state(UIDocument::UINode &node) {
  node.selection = clamp_text_selection(node, node.selection);
  node.caret.index = clamp_text_index(node, node.caret.index);
  node.text_scroll_x = std::max(0.0f, node.text_scroll_x);
}

} // namespace

UINodeId UIDocument::create_text_input(
    std::string value,
    std::string placeholder,
    std::string name
) {
  UINodeId node_id = allocate_node(NodeType::TextInput, std::move(name));
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
    style.background_color = glm::vec4(0.04f, 0.08f, 0.13f, 0.9f);
    style.border_color = glm::vec4(0.36f, 0.48f, 0.61f, 0.45f);
    style.text_color = glm::vec4(0.94f, 0.97f, 1.0f, 1.0f);
    style.placeholder_text_color = glm::vec4(0.59f, 0.68f, 0.78f, 0.88f);
    style.disabled_style.opacity = 0.6f;
  });

  return node_id;
}

void UIDocument::set_text(UINodeId node_id, std::string text) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (target->text == text) {
    return;
  }

  target->text = std::move(text);
  clamp_text_runtime_state(*target);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_placeholder(UINodeId node_id, std::string placeholder) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (target->placeholder == placeholder) {
    return;
  }

  target->placeholder = std::move(placeholder);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_autocomplete_text(
    UINodeId node_id,
    std::string autocomplete_text
) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (target->autocomplete_text == autocomplete_text) {
    return;
  }

  target->autocomplete_text = std::move(autocomplete_text);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_read_only(UINodeId node_id, bool read_only) {
  UINode *target = node(node_id);
  if (target == nullptr || target->read_only == read_only) {
    return;
  }

  target->read_only = read_only;
  m_paint_dirty = true;
}

void UIDocument::set_select_all_on_focus(
    UINodeId node_id,
    bool select_all_on_focus
) {
  UINode *target = node(node_id);
  if (target == nullptr || target->select_all_on_focus == select_all_on_focus) {
    return;
  }

  target->select_all_on_focus = select_all_on_focus;
}

void UIDocument::set_text_selection(
    UINodeId node_id,
    UITextSelection selection
) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  target->selection = clamp_text_selection(*target, selection);
  m_paint_dirty = true;
}

void UIDocument::clear_text_selection(UINodeId node_id) {
  set_text_selection(node_id, UITextSelection{});
}

void UIDocument::set_caret(UINodeId node_id, size_t index, bool active) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  target->caret.index = clamp_text_index(*target, index);
  target->caret.active = active;
  target->caret.visible = true;
  target->caret.blink_elapsed = 0.0;
  m_paint_dirty = true;
}

void UIDocument::clear_caret(UINodeId node_id) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  target->caret.active = false;
  target->caret.visible = false;
  target->caret.blink_elapsed = 0.0;
  m_paint_dirty = true;
}

void UIDocument::reset_caret_blink(UINodeId node_id) {
  UINode *target = node(node_id);
  if (target == nullptr || !target->caret.active) {
    return;
  }

  target->caret.visible = true;
  target->caret.blink_elapsed = 0.0;
  m_paint_dirty = true;
}

} // namespace astralix::ui
