#include "systems/ui-system/widgets/multi-option/chip-group.hpp"

namespace astralix::ui_system_core {

bool toggle_chip(const Target &target, size_t index, bool queue_callback) {
  if (target.document == nullptr) {
    return false;
  }

  const auto *before_node = target.document->node(target.node_id);
  if (before_node == nullptr || before_node->type != ui::NodeType::ChipGroup ||
      index >= before_node->chip_group.options.size()) {
    return false;
  }

  const bool next_selected =
      !target.document->chip_selected(target.node_id, index);
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

} // namespace astralix::ui_system_core
