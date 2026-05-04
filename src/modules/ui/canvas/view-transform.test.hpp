#include "canvas/view-transform.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, ViewTransformRoundTripsBetweenCanvasAndScreen) {
  const UIViewTransform2D transform{
      .pan = glm::vec2(12.0f, -6.0f),
      .zoom = 2.5f,
      .min_zoom = 0.25f,
      .max_zoom = 4.0f,
  };
  const glm::vec2 viewport_origin(40.0f, 30.0f);
  const glm::vec2 canvas_point(18.5f, -3.25f);

  const glm::vec2 screen_point =
      canvas_to_screen(transform, canvas_point, viewport_origin);
  const glm::vec2 round_trip =
      screen_to_canvas(transform, screen_point, viewport_origin);

  EXPECT_NEAR(round_trip.x, canvas_point.x, 1e-5f);
  EXPECT_NEAR(round_trip.y, canvas_point.y, 1e-5f);
}

TEST(UIFoundationsTest, ViewTransformClampZoomRespectsConfiguredBounds) {
  const UIViewTransform2D transform{
      .pan = glm::vec2(0.0f),
      .zoom = 1.0f,
      .min_zoom = 0.5f,
      .max_zoom = 2.0f,
  };

  EXPECT_FLOAT_EQ(clamp_zoom(transform, 0.1f), 0.5f);
  EXPECT_FLOAT_EQ(clamp_zoom(transform, 1.25f), 1.25f);
  EXPECT_FLOAT_EQ(clamp_zoom(transform, 4.0f), 2.0f);
}

TEST(UIFoundationsTest, ViewTransformZoomAroundAnchorPreservesAnchorWorldPoint) {
  const UIViewTransform2D transform{
      .pan = glm::vec2(-4.0f, 9.0f),
      .zoom = 1.5f,
      .min_zoom = 0.25f,
      .max_zoom = 4.0f,
  };
  const glm::vec2 viewport_origin(24.0f, 16.0f);
  const glm::vec2 anchor_screen(180.0f, 120.0f);
  const glm::vec2 anchor_world =
      screen_to_canvas(transform, anchor_screen, viewport_origin);

  const UIViewTransform2D next = zoom_around_screen_anchor(
      transform,
      3.0f,
      anchor_screen,
      viewport_origin
  );
  const glm::vec2 next_anchor_world =
      screen_to_canvas(next, anchor_screen, viewport_origin);

  EXPECT_NEAR(next.zoom, 3.0f, 1e-5f);
  EXPECT_NEAR(next_anchor_world.x, anchor_world.x, 1e-5f);
  EXPECT_NEAR(next_anchor_world.y, anchor_world.y, 1e-5f);
}

TEST(UIFoundationsTest, ViewTransformPanDeltaIsScaledByCurrentZoom) {
  const UIViewTransform2D transform{
      .pan = glm::vec2(0.0f),
      .zoom = 4.0f,
      .min_zoom = 0.25f,
      .max_zoom = 4.0f,
  };

  const glm::vec2 canvas_delta =
      canvas_delta_for_screen_delta(transform, glm::vec2(20.0f, -8.0f));

  EXPECT_NEAR(canvas_delta.x, 5.0f, 1e-5f);
  EXPECT_NEAR(canvas_delta.y, -2.0f, 1e-5f);
}

} // namespace
} // namespace astralix::ui
