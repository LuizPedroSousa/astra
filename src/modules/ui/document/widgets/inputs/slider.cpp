#include "document/document.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace astralix::ui {
namespace {

float sanitized_slider_step(float min_value, float max_value, float step) {
  if (step > 0.0f) {
    return step;
  }

  const float span = std::abs(max_value - min_value);
  return span > 0.0f ? span / 100.0f : 1.0f;
}

float clamp_slider_value(const UISliderState &slider, float value) {
  const float clamped = std::clamp(value, slider.min_value, slider.max_value);
  if (slider.step <= 0.0f) {
    return clamped;
  }

  const float steps = std::round((clamped - slider.min_value) / slider.step);
  return std::clamp(
      slider.min_value + steps * slider.step,
      slider.min_value,
      slider.max_value
  );
}

void normalize_slider_state(UISliderState &slider) {
  if (slider.max_value < slider.min_value) {
    std::swap(slider.min_value, slider.max_value);
  }

  slider.step =
      sanitized_slider_step(slider.min_value, slider.max_value, slider.step);
  slider.value = clamp_slider_value(slider, slider.value);
}

} // namespace

UINodeId UIDocument::create_slider(
    float value,
    float min_value,
    float max_value,
    float step
) {
  UINodeId node_id = allocate_node(NodeType::Slider);
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->focusable = true;
    node->slider = UISliderState{
        .value = value,
        .min_value = min_value,
        .max_value = max_value,
        .step = step,
    };
    normalize_slider_state(node->slider);
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.padding = UIEdges::symmetric(12.0f, 10.0f);
    style.border_radius = 12.0f;
    style.border_width = 1.0f;
    style.cursor = CursorStyle::Pointer;
    style.background_color = glm::vec4(0.04f, 0.08f, 0.13f, 0.8f);
    style.border_color = glm::vec4(0.34f, 0.46f, 0.6f, 0.42f);
    style.hovered_style.border_color = glm::vec4(0.5f, 0.64f, 0.82f, 0.72f);
    style.pressed_style.border_color = glm::vec4(0.68f, 0.82f, 1.0f, 0.92f);
    style.focused_style.border_color = glm::vec4(0.8f, 0.9f, 1.0f, 0.95f);
    style.focused_style.border_width = 2.0f;
    style.disabled_style.opacity = 0.55f;
  });

  return node_id;
}

void UIDocument::set_slider_value(UINodeId node_id, float value) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Slider) {
    return;
  }

  const float clamped = clamp_slider_value(target->slider, value);
  if (target->slider.value == clamped) {
    return;
  }

  target->slider.value = clamped;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

float UIDocument::slider_value(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Slider) {
    return 0.0f;
  }

  return target->slider.value;
}

void UIDocument::set_slider_range(
    UINodeId node_id,
    float min_value,
    float max_value,
    float step
) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Slider) {
    return;
  }

  const UISliderState previous = target->slider;
  target->slider.min_value = min_value;
  target->slider.max_value = max_value;
  target->slider.step = step;
  normalize_slider_state(target->slider);

  if (target->slider.min_value == previous.min_value &&
      target->slider.max_value == previous.max_value &&
      target->slider.step == previous.step &&
      target->slider.value == previous.value) {
    return;
  }

  m_layout_dirty = true;
  m_paint_dirty = true;
}

} // namespace astralix::ui
