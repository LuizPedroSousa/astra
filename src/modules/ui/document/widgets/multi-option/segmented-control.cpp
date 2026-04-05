#include "document/document.hpp"

#include <algorithm>
#include <utility>

namespace astralix::ui {
namespace {

void normalize_segmented_control_state(UISegmentedControlState &segmented) {
  if (segmented.options.empty()) {
    segmented.selected_index = 0u;
    return;
  }

  segmented.selected_index =
      std::min(segmented.selected_index, segmented.options.size() - 1u);
}

bool vec4_equal(const glm::vec4 &lhs, const glm::vec4 &rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

bool accent_colors_equal(
    const std::vector<glm::vec4> &lhs,
    const std::vector<glm::vec4> &rhs
) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t index = 0u; index < lhs.size(); ++index) {
    if (!vec4_equal(lhs[index], rhs[index])) {
      return false;
    }
  }

  return true;
}

} // namespace

UINodeId UIDocument::create_segmented_control(
    std::vector<std::string> options,
    size_t selected_index
) {
  UINodeId node_id = allocate_node(NodeType::SegmentedControl);
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->focusable = true;
    node->segmented_control.options = std::move(options);
    node->segmented_control.selected_index = selected_index;
    normalize_segmented_control_state(node->segmented_control);
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.padding = UIEdges::all(0.0f);
    style.border_radius = 0.0f;
    style.border_width = 0.0f;
    style.cursor = CursorStyle::Pointer;
    style.background_color = glm::vec4(0.0f);
    style.border_color = glm::vec4(1.0f);
    style.text_color = glm::vec4(1.0f);
    style.control_gap = 4.0f;
    style.focused_style.border_color = glm::vec4(0.74f, 0.84f, 0.98f, 0.95f);
    style.focused_style.border_width = 2.0f;
    style.disabled_style.opacity = 0.6f;
  });

  return node_id;
}

void UIDocument::set_segmented_options(
    UINodeId node_id,
    std::vector<std::string> options
) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::SegmentedControl) {
    return;
  }

  if (target->segmented_control.options == options) {
    return;
  }

  target->segmented_control.options = std::move(options);
  normalize_segmented_control_state(target->segmented_control);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

const std::vector<std::string> *
UIDocument::segmented_options(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::SegmentedControl) {
    return nullptr;
  }

  return &target->segmented_control.options;
}

void UIDocument::set_segmented_selected_index(
    UINodeId node_id,
    size_t selected_index
) {
  UINode *target = node(node_id);

  if (target == nullptr || target->type != NodeType::SegmentedControl) {
    return;
  }

  const size_t previous_selected = target->segmented_control.selected_index;
  target->segmented_control.selected_index = selected_index;
  normalize_segmented_control_state(target->segmented_control);

  if (target->segmented_control.selected_index == previous_selected) {
    return;
  }

  m_paint_dirty = true;
}

size_t UIDocument::segmented_selected_index(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::SegmentedControl) {
    return 0u;
  }

  return target->segmented_control.selected_index;
}

void UIDocument::set_segmented_item_accent_colors(
    UINodeId node_id,
    std::vector<glm::vec4> colors
) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::SegmentedControl) {
    return;
  }

  if (accent_colors_equal(target->segmented_control.item_accent_colors, colors)) {
    return;
  }

  target->segmented_control.item_accent_colors = std::move(colors);
  m_paint_dirty = true;
}

} // namespace astralix::ui
