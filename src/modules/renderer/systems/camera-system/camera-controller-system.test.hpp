#include "camera-controller-system.hpp"

#include <gtest/gtest.h>

namespace astralix::scene {
namespace {

TEST(CameraControllerTest, FreeCameraMovesAndUpdatesMatrices) {
  ecs::World world;
  auto entity = world.spawn("camera");
  entity.emplace<Transform>();
  entity.emplace<rendering::Camera>();
  entity.emplace<CameraController>();

  update_camera_controllers(world, CameraControllerInput{
                                       .forward = true,
                                       .mouse_delta = glm::vec2(10.0f, -5.0f),
                                       .dt = 0.5f,
                                       .aspect_ratio = 16.0f / 9.0f,
                                   });

  auto *transform = entity.get<Transform>();
  auto *camera = entity.get<rendering::Camera>();

  ASSERT_NE(transform, nullptr);
  ASSERT_NE(camera, nullptr);
  EXPECT_NE(transform->position, glm::vec3(0.0f));
  EXPECT_NE(camera->view_matrix[0][0], 0.0f);
  EXPECT_NE(camera->projection_matrix[0][0], 0.0f);
}

TEST(CameraControllerTest, OrbitalCameraTracksTargetTransform) {
  ecs::World world;
  auto target = world.spawn("target");
  target.emplace<Transform>(Transform{.position = glm::vec3(2.0f, 0.0f, 0.0f)});

  auto camera_entity = world.spawn("camera");
  camera_entity.emplace<Transform>();
  camera_entity.emplace<rendering::Camera>();
  camera_entity.emplace<CameraController>(
      CameraController{.mode = CameraControllerMode::Orbital,
                       .orbit_distance = 3.0f,
                       .target = target.id()});

  update_camera_controllers(world, CameraControllerInput{
                                       .mouse_delta = glm::vec2(0.0f, 0.0f),
                                       .dt = 0.016f,
                                       .aspect_ratio = 1.0f,
                                   });

  auto *camera_transform = camera_entity.get<Transform>();
  auto *camera = camera_entity.get<rendering::Camera>();

  ASSERT_NE(camera_transform, nullptr);
  ASSERT_NE(camera, nullptr);
  EXPECT_NE(camera_transform->position, target.get<Transform>()->position);
  EXPECT_NE(camera->front, glm::vec3(0.0f, 0.0f, 1.0f));
}

} // namespace
} // namespace astralix::scene
