#include "document/document.hpp"

#include <utility>

namespace astralix::ui {
namespace {

void normalize_chip_group_state(UIChipGroupState &chip_group) {
  if (chip_group.options.empty()) {
    chip_group.selected.clear();
    return;
  }

  if (chip_group.selected.size() != chip_group.options.size()) {
    const size_t previous_size = chip_group.selected.size();
    chip_group.selected.resize(chip_group.options.size(), true);
    for (size_t index = previous_size; index < chip_group.selected.size();
         ++index) {
      chip_group.selected[index] = true;
    }
  }
}

} // namespace

UINodeId UIDocument::create_chip_group(
    std::vector<std::string> options,
    std::vector<bool> selected
) {
  UINodeId node_id = allocate_node(NodeType::ChipGroup);
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->chip_group.options = std::move(options);
    node->chip_group.selected = std::move(selected);
    normalize_chip_group_state(node->chip_group);
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.padding = UIEdges::all(0.0f);
    style.border_radius = 0.0f;
    style.cursor = CursorStyle::Pointer;
    style.text_color = glm::vec4(0.94f, 0.97f, 1.0f, 1.0f);
    style.control_gap = 8.0f;
    style.disabled_style.opacity = 0.6f;
  });

  return node_id;
}

void UIDocument::set_chip_options(
    UINodeId node_id,
    std::vector<std::string> options,
    std::vector<bool> selected
) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::ChipGroup) {
    return;
  }

  UIChipGroupState next_state{
      .options = std::move(options),
      .selected = std::move(selected),
  };
  normalize_chip_group_state(next_state);

  if (target->chip_group.options == next_state.options &&
      target->chip_group.selected == next_state.selected) {
    return;
  }

  target->chip_group = std::move(next_state);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

const std::vector<std::string> *
UIDocument::chip_options(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::ChipGroup) {
    return nullptr;
  }

  return &target->chip_group.options;
}

void UIDocument::set_chip_selected(UINodeId node_id, size_t index, bool selected) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::ChipGroup) {
    return;
  }

  normalize_chip_group_state(target->chip_group);
  if (index >= target->chip_group.selected.size() ||
      target->chip_group.selected[index] == selected) {
    return;
  }

  target->chip_group.selected[index] = selected;
  m_paint_dirty = true;
}

bool UIDocument::chip_selected(UINodeId node_id, size_t index) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::ChipGroup) {
    return false;
  }

  if (index >= target->chip_group.selected.size()) {
    return false;
  }

  return target->chip_group.selected[index];
}

} // namespace astralix::ui
