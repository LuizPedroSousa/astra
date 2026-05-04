#include "canvas/view-transform.hpp"

#include <algorithm>

namespace astralix::ui {
namespace {

float ordered_min_zoom(const UIViewTransform2D &transform) {
  return std::min(transform.min_zoom, transform.max_zoom);
}

float ordered_max_zoom(const UIViewTransform2D &transform) {
  return std::max(transform.min_zoom, transform.max_zoom);
}

} // namespace

glm::vec2 canvas_to_screen(
    const UIViewTransform2D &transform,
    glm::vec2 canvas_point,
    glm::vec2 viewport_origin
) {
  const float zoom = clamp_zoom(transform, transform.zoom);
  return viewport_origin + (canvas_point + transform.pan) * zoom;
}

glm::vec2 screen_to_canvas(
    const UIViewTransform2D &transform,
    glm::vec2 screen_point,
    glm::vec2 viewport_origin
) {
  const float zoom = clamp_zoom(transform, transform.zoom);
  return (screen_point - viewport_origin) / zoom - transform.pan;
}

float clamp_zoom(const UIViewTransform2D &transform, float zoom) {
  return std::max(
      0.0001f,
      std::clamp(zoom, ordered_min_zoom(transform), ordered_max_zoom(transform))
  );
}

glm::vec2 canvas_delta_for_screen_delta(
    const UIViewTransform2D &transform,
    glm::vec2 screen_delta
) {
  return screen_delta / clamp_zoom(transform, transform.zoom);
}

UIViewTransform2D zoom_around_screen_anchor(
    const UIViewTransform2D &transform,
    float zoom,
    glm::vec2 anchor_screen,
    glm::vec2 viewport_origin
) {
  UIViewTransform2D next = transform;
  const glm::vec2 anchor_world =
      screen_to_canvas(transform, anchor_screen, viewport_origin);
  next.zoom = clamp_zoom(transform, zoom);
  next.pan = (anchor_screen - viewport_origin) / next.zoom - anchor_world;
  return next;
}

} // namespace astralix::ui
