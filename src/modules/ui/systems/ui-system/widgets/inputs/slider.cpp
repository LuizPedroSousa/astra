#include "systems/ui-system/widgets/inputs/slider.hpp"

#include <algorithm>

namespace astralix::ui_system_core {
namespace {

float slider_value_from_pointer(
    const ui::UIDocument::UINode &node,
    glm::vec2 pointer
) {
  const auto &track = node.layout.slider.track_rect;
  if (track.width <= 0.0f) {
    return node.slider.min_value;
  }

  const float ratio =
      std::clamp((pointer.x - track.x) / track.width, 0.0f, 1.0f);
  return node.slider.min_value +
         (node.slider.max_value - node.slider.min_value) * ratio;
}

} // namespace

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

} // namespace astralix::ui_system_core
