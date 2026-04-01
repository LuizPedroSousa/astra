#include "systems/ui-system/widgets/multi-option/segmented-control.hpp"

#include <algorithm>

namespace astralix::ui_system_core {

bool select_segmented_option(const Target &target, size_t index, bool queue_callback) {
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
          [callback, clamped, label]() { callback(clamped, label); }
      );
    }
  }

  return changed;
}

bool move_segmented_selection(const Target &target, int direction, bool queue_callback) {
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
      static_cast<int>(node->segmented_control.selected_index) + direction,
      0,
      max_index
  );
  return select_segmented_option(
      target, static_cast<size_t>(next_index), queue_callback
  );
}

} // namespace astralix::ui_system_core
