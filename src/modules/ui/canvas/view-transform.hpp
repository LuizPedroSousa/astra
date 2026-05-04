#pragma once

#include "types.hpp"

namespace astralix::ui {

glm::vec2 canvas_to_screen(
    const UIViewTransform2D &transform,
    glm::vec2 canvas_point,
    glm::vec2 viewport_origin = glm::vec2(0.0f)
);

glm::vec2 screen_to_canvas(
    const UIViewTransform2D &transform,
    glm::vec2 screen_point,
    glm::vec2 viewport_origin = glm::vec2(0.0f)
);

float clamp_zoom(const UIViewTransform2D &transform, float zoom);

glm::vec2 canvas_delta_for_screen_delta(
    const UIViewTransform2D &transform,
    glm::vec2 screen_delta
);

UIViewTransform2D zoom_around_screen_anchor(
    const UIViewTransform2D &transform,
    float zoom,
    glm::vec2 anchor_screen,
    glm::vec2 viewport_origin = glm::vec2(0.0f)
);

} // namespace astralix::ui
