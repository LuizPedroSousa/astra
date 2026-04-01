#include "systems/ui-system/widgets/inputs/select.hpp"

#include <algorithm>

namespace astralix::ui_system_core {
namespace {

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

  const ui::UINodeId open_popup_id = entry.document->open_popup_node();
  if (open_popup_id == ui::k_invalid_node_id) {
    return;
  }

  if (auto *node = entry.document->node(open_popup_id); node != nullptr &&
      node->type == ui::NodeType::Select) {
    if (node->layout.select.hovered_option_index.has_value()) {
      entry.document->mark_paint_dirty();
    }
    node->layout.select.hovered_option_index.reset();
  }
}

} // namespace

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

bool confirm_select_option(const Target &target, size_t index, bool queue_callback) {
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
          [callback, clamped, label]() { callback(clamped, label); }
      );
    }
  }

  return changed;
}

void apply_select_visual_state(
    const std::vector<RootEntry> &roots,
    const std::optional<PointerHit> &hover_hit
) {
  for (const RootEntry &entry : roots) {
    clear_select_visual_state(entry);
  }

  if (!hover_hit.has_value() || !hover_hit->item_index.has_value() ||
      hover_hit->target.document == nullptr) {
    return;
  }

  auto *node = hover_hit->target.document->node(hover_hit->target.node_id);
  if (node == nullptr || hover_hit->part != ui::UIHitPart::SelectOption ||
      node->type != ui::NodeType::Select) {
    return;
  }

  node->layout.select.hovered_option_index = hover_hit->item_index;
  node->select.highlighted_index = *hover_hit->item_index;
  hover_hit->target.document->mark_paint_dirty();
}

} // namespace astralix::ui_system_core
