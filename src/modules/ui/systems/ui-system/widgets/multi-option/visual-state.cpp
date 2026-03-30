#include "systems/ui-system/widgets/multi-option/visual-state.hpp"

namespace astralix::ui_system_core {
namespace {

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

} // namespace

void apply_item_control_visual_state(
    const std::vector<RootEntry> &roots,
    const std::optional<PointerHit> &hover_hit,
    const std::optional<Target> &active_target,
    ui::UIHitPart active_part,
    const std::optional<size_t> &active_item_index
) {
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
