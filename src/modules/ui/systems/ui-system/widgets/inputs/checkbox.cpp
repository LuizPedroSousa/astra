#include "systems/ui-system/widgets/inputs/checkbox.hpp"

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
        [callback, checked]() { callback(checked); }
    );
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

} // namespace astralix::ui_system_core
