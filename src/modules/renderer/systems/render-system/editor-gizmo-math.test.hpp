#include "tools/viewport/gizmo-math.hpp"

#include "glm/gtc/matrix_transform.hpp"
#include <gtest/gtest.h>

#include <cmath>

namespace astralix::editor::gizmo {
namespace {

CameraFrame make_perspective_camera() {
  return CameraFrame{
      .position = glm::vec3(0.0f, 0.0f, 5.0f),
      .forward = glm::vec3(0.0f, 0.0f, -1.0f),
      .up = glm::vec3(0.0f, 1.0f, 0.0f),
      .view = glm::lookAt(
          glm::vec3(0.0f, 0.0f, 5.0f),
          glm::vec3(0.0f, 0.0f, 0.0f),
          glm::vec3(0.0f, 1.0f, 0.0f)
      ),
      .projection = glm::perspective(
          glm::radians(60.0f),
          1.0f,
          0.1f,
          100.0f
      ),
      .orthographic = false,
      .fov_degrees = 60.0f,
      .orthographic_scale = 10.0f,
  };
}

TEST(EditorGizmoMathTest, PerspectiveScaleTracksViewportHeight) {
  const CameraFrame camera = make_perspective_camera();
  const float expected_world_units_per_pixel =
      (2.0f * 5.0f * std::tan(glm::radians(30.0f))) / 1000.0f;

  EXPECT_NEAR(
      world_units_per_pixel(camera, glm::vec3(0.0f), 1000.0f),
      expected_world_units_per_pixel,
      0.0001f
  );
  EXPECT_NEAR(
      gizmo_scale_world(camera, glm::vec3(0.0f), 1000.0f),
      expected_world_units_per_pixel * k_target_pixel_size,
      0.0001f
  );
  EXPECT_NEAR(
      gizmo_scale_world(camera, glm::vec3(0.0f), 500.0f),
      gizmo_scale_world(camera, glm::vec3(0.0f), 1000.0f) * 2.0f,
      0.0001f
  );
}

TEST(EditorGizmoMathTest, OrthographicScaleUsesOrthographicExtent) {
  CameraFrame camera = make_perspective_camera();
  camera.orthographic = true;
  camera.orthographic_scale = 8.0f;

  EXPECT_NEAR(
      world_units_per_pixel(camera, glm::vec3(0.0f), 400.0f),
      0.04f,
      0.0001f
  );
  EXPECT_NEAR(
      gizmo_scale_world(camera, glm::vec3(0.0f), 400.0f),
      3.84f,
      0.0001f
  );
}

TEST(EditorGizmoMathTest, ProjectionAndRayHelpersMapViewportCenterToPivot) {
  const CameraFrame camera = make_perspective_camera();
  const ui::UIRect viewport_rect{
      .x = 50.0f,
      .y = 100.0f,
      .width = 800.0f,
      .height = 600.0f,
  };

  const ProjectedPoint projected =
      project_world_point(camera, viewport_rect, glm::vec3(0.0f));
  ASSERT_TRUE(projected.valid);
  EXPECT_NEAR(projected.position.x, 450.0f, 0.001f);
  EXPECT_NEAR(projected.position.y, 400.0f, 0.001f);

  const ScreenRay ray = screen_ray(
      camera,
      viewport_rect,
      glm::vec2(450.0f, 400.0f)
  );
  const auto hit =
      intersect_ray_plane(ray, glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  ASSERT_TRUE(hit.has_value());
  EXPECT_NEAR(hit->x, 0.0f, 0.001f);
  EXPECT_NEAR(hit->y, 0.0f, 0.001f);
  EXPECT_NEAR(hit->z, 0.0f, 0.001f);
}

TEST(EditorGizmoMathTest, SignedAngleUsesAxisOrientation) {
  EXPECT_NEAR(
      signed_angle_on_axis(
          glm::vec3(1.0f, 0.0f, 0.0f),
          glm::vec3(0.0f, 1.0f, 0.0f),
          glm::vec3(0.0f, 0.0f, 1.0f)
      ),
      glm::half_pi<float>(),
      0.0001f
  );
}

TEST(EditorGizmoMathTest, ScaleFactorClampsToPositiveMinimum) {
  EXPECT_FLOAT_EQ(scale_factor_from_axis_delta(-2.0f, 1.0f), k_min_scale_component);
  EXPECT_NEAR(scale_factor_from_axis_delta(0.5f, 2.0f), 1.25f, 0.0001f);
}

TEST(EditorGizmoMathTest, PicksProjectedAxisHandle) {
  const CameraFrame camera = make_perspective_camera();
  const ui::UIRect viewport_rect{
      .x = 0.0f,
      .y = 0.0f,
      .width = 800.0f,
      .height = 600.0f,
  };
  const auto segments =
      build_line_segments(EditorGizmoMode::Translate, glm::vec3(0.0f), 1.0f);

  ASSERT_FALSE(segments.empty());
  const ProjectedPoint start =
      project_world_point(camera, viewport_rect, segments.front().start);
  const ProjectedPoint end =
      project_world_point(camera, viewport_rect, segments.front().end);
  ASSERT_TRUE(start.valid);
  ASSERT_TRUE(end.valid);

  const glm::vec2 midpoint = (start.position + end.position) * 0.5f;
  const auto handle = pick_handle(segments, camera, viewport_rect, midpoint);

  ASSERT_TRUE(handle.has_value());
  EXPECT_EQ(*handle, EditorGizmoHandle::TranslateX);
}

TEST(EditorGizmoStoreTest, UsesPanelOrWindowInteractionRectBasedOnCaptureMode) {
  EditorGizmoStore store;
  store.set_panel_rect(ui::UIRect{
      .x = 10.0f,
      .y = 20.0f,
      .width = 300.0f,
      .height = 200.0f,
  });
  store.set_window_rect(ui::UIRect{
      .x = 0.0f,
      .y = 0.0f,
      .width = 1280.0f,
      .height = 720.0f,
  });

  ASSERT_TRUE(store.interaction_rect().has_value());
  EXPECT_FLOAT_EQ(store.interaction_rect()->x, 10.0f);
  EXPECT_TRUE(store.point_in_interaction_region(glm::vec2(50.0f, 50.0f)));

  store.set_window_capture_enabled(true);
  store.set_blocked_rects({
      ui::UIRect{
          .x = 40.0f,
          .y = 40.0f,
          .width = 120.0f,
          .height = 90.0f,
      },
  });

  ASSERT_TRUE(store.interaction_rect().has_value());
  EXPECT_FLOAT_EQ(store.interaction_rect()->width, 1280.0f);
  EXPECT_TRUE(store.point_in_interaction_region(glm::vec2(10.0f, 10.0f)));
  EXPECT_FALSE(store.point_in_interaction_region(glm::vec2(60.0f, 60.0f)));
}

} // namespace
} // namespace astralix::editor::gizmo
