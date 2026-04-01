#include "systems/ui-system/widgets/actions/pressable.hpp"

namespace astralix::ui_system_core {
namespace {

template <typename CallbackAccessor>
void queue_node_callback(const Target &target, CallbackAccessor callback_accessor) {
  if (target.document == nullptr) {
    return;
  }

  const auto *node = target.document->node(target.node_id);
  if (node == nullptr) {
    return;
  }

  auto callback = callback_accessor(*node);
  if (callback) {
    target.document->queue_callback(callback);
  }
}

} // namespace

void queue_press_callback(const Target &target) {
  queue_node_callback(target, [](const ui::UIDocument::UINode &node) {
    return node.on_press;
  });
}

void queue_release_callback(const Target &target) {
  queue_node_callback(target, [](const ui::UIDocument::UINode &node) {
    return node.on_release;
  });
}

void queue_click_callback(const Target &target) {
  queue_node_callback(target, [](const ui::UIDocument::UINode &node) {
    return node.on_click;
  });
}

} // namespace astralix::ui_system_core
