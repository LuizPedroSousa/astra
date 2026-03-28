#include "systems/ui-system/controls.hpp"

#include <algorithm>

namespace astralix::ui_system_core {
namespace {

void queue_checkbox_toggle(const Target &target, bool checked) {
  if (target.document == nullptr) {
    return;
  }

  const auto *node = target.document->node(target.node_id);
  if (node == nullptr) {
    return;
  }

  if (node->on_click) {
    target.document->queue_callback(node->on_click);
  }

  if (node->on_toggle) {
    auto callback = node->on_toggle;
    target.document->queue_callback(
        [callback, checked]() { callback(checked); });
  }
}

void clear_item_visual_state(const RootEntry &entry) {
  if (entry.document == nullptr) {
    return;
  }

  bool changed = false;
  for (ui::UINodeId node_id : entry.document->root_to_leaf_order()) {
    auto *node = entry.document->node(node_id);
    if (node == nullptr) {
      continue;
    }

    if (node->type == ui::NodeType::SegmentedControl) {
      if (node->layout.segmented_control.hovered_item_index.has_value() ||
          node->layout.segmented_control.active_item_index.has_value()) {
        node->layout.segmented_control.hovered_item_index.reset();
        node->layout.segmented_control.active_item_index.reset();
        changed = true;
      }
    } else if (node->type == ui::NodeType::ChipGroup) {
      if (node->layout.chip_group.hovered_item_index.has_value() ||
          node->layout.chip_group.active_item_index.has_value()) {
        node->layout.chip_group.hovered_item_index.reset();
        node->layout.chip_group.active_item_index.reset();
        changed = true;
      }
    }
  }

  if (changed) {
    entry.document->mark_paint_dirty();
  }
}

float slider_value_from_pointer(const ui::UIDocument::UINode &node,
                                glm::vec2 pointer) {
  const auto &track = node.layout.slider.track_rect;
  if (track.width <= 0.0f) {
    return node.slider.min_value;
  }

  const float ratio =
      std::clamp((pointer.x - track.x) / track.width, 0.0f, 1.0f);
  return node.slider.min_value +
         (node.slider.max_value - node.slider.min_value) * ratio;
}

void set_select_highlight(const Target &target, size_t index) {
  if (target.document == nullptr) {
    return;
  }

  auto *node = target.document->node(target.node_id);
  if (node == nullptr || node->type != ui::NodeType::Select ||
      node->select.options.empty()) {
    return;
  }

  const size_t clamped = std::min(index, node->select.options.size() - 1u);
  if (node->select.highlighted_index == clamped) {
    return;
  }

  node->select.highlighted_index = clamped;
  target.document->mark_paint_dirty();
}

void clear_select_visual_state(const RootEntry &entry) {
  if (entry.document == nullptr) {
    return;
  }

  const ui::UINodeId open_select_id = entry.document->open_select_node();
  if (open_select_id == ui::k_invalid_node_id) {
    return;
  }

  if (auto *node = entry.document->node(open_select_id); node != nullptr) {
    if (node->layout.select.hovered_option_index.has_value()) {
      entry.document->mark_paint_dirty();
    }
    node->layout.select.hovered_option_index.reset();
  }
}

} // namespace

bool toggle_checkbox_value(const Target &target, bool queue_callback) {
  if (target.document == nullptr) {
    return false;
  }

  const bool next_checked = !target.document->checked(target.node_id);
  const bool changed = target.document->checked(target.node_id) != next_checked;
  target.document->set_checked(target.node_id, next_checked);
  if (changed && queue_callback) {
    queue_checkbox_toggle(target, next_checked);
  }

  return changed;
}

bool apply_slider_value(const Target &target, float value, bool queue_change) {
  if (target.document == nullptr) {
    return false;
  }

  const float before = target.document->slider_value(target.node_id);
  target.document->set_slider_value(target.node_id, value);
  const float after = target.document->slider_value(target.node_id);
  const bool changed = before != after;

  if (changed && queue_change) {
    const auto *node = target.document->node(target.node_id);
    if (node != nullptr && node->on_value_change) {
      auto callback = node->on_value_change;
      target.document->queue_callback([callback, after]() { callback(after); });
    }
  }

  return changed;
}

void update_slider_drag(const Target &target, glm::vec2 pointer) {
  if (target.document == nullptr) {
    return;
  }

  const auto *node = target.document->node(target.node_id);
  if (node == nullptr || node->type != ui::NodeType::Slider) {
    return;
  }

  apply_slider_value(target, slider_value_from_pointer(*node, pointer), true);
}

bool select_segmented_option(const Target &target, size_t index,
                             bool queue_callback) {
  if (target.document == nullptr) {
    return false;
  }

  const auto *before_node = target.document->node(target.node_id);
  if (before_node == nullptr ||
      before_node->type != ui::NodeType::SegmentedControl ||
      before_node->segmented_control.options.empty()) {
    return false;
  }

  const size_t clamped =
      std::min(index, before_node->segmented_control.options.size() - 1u);
  const bool changed =
      target.document->segmented_selected_index(target.node_id) != clamped;
  target.document->set_segmented_selected_index(target.node_id, clamped);

  if (queue_callback) {
    if (const auto *node = target.document->node(target.node_id);
        node != nullptr && node->on_select &&
        clamped < node->segmented_control.options.size()) {
      auto callback = node->on_select;
      auto label = node->segmented_control.options[clamped];
      target.document->queue_callback(
          [callback, clamped, label]() { callback(clamped, label); });
    }
  }

  return changed;
}

bool move_segmented_selection(const Target &target, int direction,
                              bool queue_callback) {
  if (target.document == nullptr) {
    return false;
  }

  const auto *node = target.document->node(target.node_id);
  if (node == nullptr || node->type != ui::NodeType::SegmentedControl ||
      node->segmented_control.options.empty()) {
    return false;
  }

  const int max_index =
      static_cast<int>(node->segmented_control.options.size()) - 1;
  const int next_index = std::clamp(
      static_cast<int>(node->segmented_control.selected_index) + direction, 0,
      max_index);
  return select_segmented_option(target, static_cast<size_t>(next_index),
                                 queue_callback);
}

bool toggle_chip(const Target &target, size_t index, bool queue_callback) {
  if (target.document == nullptr) {
    return false;
  }

  const auto *before_node = target.document->node(target.node_id);
  if (before_node == nullptr || before_node->type != ui::NodeType::ChipGroup ||
      index >= before_node->chip_group.options.size()) {
    return false;
  }

  const bool next_selected = !target.document->chip_selected(target.node_id, index);
  const bool changed =
      target.document->chip_selected(target.node_id, index) != next_selected;
  target.document->set_chip_selected(target.node_id, index, next_selected);

  if (changed && queue_callback) {
    if (const auto *node = target.document->node(target.node_id);
        node != nullptr && node->on_chip_toggle &&
        index < node->chip_group.options.size()) {
      auto callback = node->on_chip_toggle;
      auto label = node->chip_group.options[index];
      target.document->queue_callback([callback, index, label, next_selected]() {
        callback(index, label, next_selected);
      });
    }
  }

  return changed;
}

void move_select_highlight(const Target &target, int direction) {
  if (target.document == nullptr) {
    return;
  }

  const auto *node = target.document->node(target.node_id);
  if (node == nullptr || node->type != ui::NodeType::Select ||
      node->select.options.empty()) {
    return;
  }

  const int max_index = static_cast<int>(node->select.options.size()) - 1;
  const int next_index =
      std::clamp(static_cast<int>(node->select.highlighted_index) + direction,
                 0, max_index);
  set_select_highlight(target, static_cast<size_t>(next_index));
}

bool confirm_select_option(const Target &target, size_t index,
                           bool queue_callback) {
  if (target.document == nullptr) {
    return false;
  }

  const auto *before_node = target.document->node(target.node_id);
  if (before_node == nullptr || before_node->type != ui::NodeType::Select ||
      before_node->select.options.empty()) {
    return false;
  }

  const size_t clamped =
      std::min(index, before_node->select.options.size() - 1u);
  const bool changed =
      target.document->selected_index(target.node_id) != clamped;
  target.document->set_selected_index(target.node_id, clamped);
  target.document->set_select_open(target.node_id, false);

  if (queue_callback) {
    if (const auto *node = target.document->node(target.node_id);
        node != nullptr && node->on_select &&
        clamped < node->select.options.size()) {
      auto callback = node->on_select;
      auto label = node->select.options[clamped];
      target.document->queue_callback(
          [callback, clamped, label]() { callback(clamped, label); });
    }
  }

  return changed;
}

void apply_select_visual_state(const std::vector<RootEntry> &roots,
                               const std::optional<PointerHit> &hover_hit) {
  for (const RootEntry &entry : roots) {
    clear_select_visual_state(entry);
  }

  if (!hover_hit.has_value() ||
      hover_hit->part != ui::UIHitPart::SelectOption ||
      !hover_hit->item_index.has_value() ||
      hover_hit->target.document == nullptr) {
    return;
  }

  auto *node = hover_hit->target.document->node(hover_hit->target.node_id);
  if (node == nullptr || node->type != ui::NodeType::Select) {
    return;
  }

  node->layout.select.hovered_option_index = hover_hit->item_index;
  node->select.highlighted_index = *hover_hit->item_index;
  hover_hit->target.document->mark_paint_dirty();
}

void apply_item_control_visual_state(
    const std::vector<RootEntry> &roots,
    const std::optional<PointerHit> &hover_hit,
    const std::optional<Target> &active_target, ui::UIHitPart active_part,
    const std::optional<size_t> &active_item_index) {
  for (const RootEntry &entry : roots) {
    clear_item_visual_state(entry);
  }

  if (hover_hit.has_value() && hover_hit->item_index.has_value() &&
      hover_hit->target.document != nullptr) {
    auto *node = hover_hit->target.document->node(hover_hit->target.node_id);
    if (node != nullptr &&
        hover_hit->part == ui::UIHitPart::SegmentedControlItem &&
        node->type == ui::NodeType::SegmentedControl) {
      node->layout.segmented_control.hovered_item_index = hover_hit->item_index;
      hover_hit->target.document->mark_paint_dirty();
    } else if (node != nullptr && hover_hit->part == ui::UIHitPart::ChipItem &&
               node->type == ui::NodeType::ChipGroup) {
      node->layout.chip_group.hovered_item_index = hover_hit->item_index;
      hover_hit->target.document->mark_paint_dirty();
    }
  }

  if (!active_target.has_value() || active_target->document == nullptr ||
      !active_item_index.has_value()) {
    return;
  }

  auto *node = active_target->document->node(active_target->node_id);
  if (node == nullptr) {
    return;
  }

  if (active_part == ui::UIHitPart::SegmentedControlItem &&
      node->type == ui::NodeType::SegmentedControl) {
    node->layout.segmented_control.active_item_index = active_item_index;
    active_target->document->mark_paint_dirty();
  } else if (active_part == ui::UIHitPart::ChipItem &&
             node->type == ui::NodeType::ChipGroup) {
    node->layout.chip_group.active_item_index = active_item_index;
    active_target->document->mark_paint_dirty();
  }
}

} // namespace astralix::ui_system_core
