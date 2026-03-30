#include "light-frame.hpp"

#include <gtest/gtest.h>

namespace astralix::rendering {
namespace {

TEST(LightFrameTest, CollectsDirectionalPointAndSpotLightData) {
  ecs::World world;

  auto camera = world.spawn("camera");
  camera.emplace<scene::Transform>(scene::Transform{.position = glm::vec3(1.0f, 2.0f, 3.0f)});
  camera.emplace<Camera>(Camera{.front = glm::vec3(0.0f, 0.0f, -1.0f)});
  camera.emplace<MainCamera>();

  auto sun = world.spawn("sun");
  sun.emplace<scene::Transform>(scene::Transform{.position = glm::vec3(-4.0f, 8.0f, -3.0f)});
  sun.emplace<Light>(Light{.type = LightType::Directional, .color = glm::vec3(1.0f, 0.5f, 0.25f), .intensity = 2.0f});
  sun.emplace<DirectionalShadowSettings>(DirectionalShadowSettings{
      .ortho_extent = 15.0f,
      .near_plane = 0.5f,
      .far_plane = 150.0f,
  });

  auto point = world.spawn("lamp");
  point.emplace<scene::Transform>(scene::Transform{.position = glm::vec3(5.0f, 1.0f, -2.0f)});
  point.emplace<Light>(Light{.type = LightType::Point, .color = glm::vec3(0.5f, 1.0f, 0.25f), .intensity = 3.0f});
  point.emplace<PointLightAttenuation>(PointLightAttenuation{
      .constant = 2.0f,
      .linear = 0.1f,
      .quadratic = 0.01f,
  });

  auto spot = world.spawn("flashlight");
  spot.emplace<scene::Transform>();
  spot.emplace<Light>(Light{.type = LightType::Spot, .color = glm::vec3(0.25f, 0.5f, 1.0f), .intensity = 1.5f});
  spot.emplace<SpotLightTarget>(SpotLightTarget{.camera = camera.id()});
  spot.emplace<SpotLightCone>(SpotLightCone{
      .inner_cutoff_cos = 0.8f,
      .outer_cutoff_cos = 0.6f,
  });

  const auto frame = collect_light_frame(world);

  EXPECT_TRUE(frame.directional.valid);
  EXPECT_EQ(frame.directional.position, glm::vec3(-4.0f, 8.0f, -3.0f));
  EXPECT_FLOAT_EQ(frame.directional.near_plane, 0.5f);
  EXPECT_FLOAT_EQ(frame.directional.far_plane, 150.0f);
  EXPECT_NE(frame.directional.light_space_matrix, glm::mat4(1.0f));

  EXPECT_TRUE(frame.point_lights[0].valid);
  EXPECT_EQ(frame.point_lights[0].position, glm::vec3(5.0f, 1.0f, -2.0f));
  EXPECT_FLOAT_EQ(frame.point_lights[0].constant, 2.0f);
  EXPECT_FLOAT_EQ(frame.point_lights[0].linear, 0.1f);
  EXPECT_FLOAT_EQ(frame.point_lights[0].quadratic, 0.01f);

  EXPECT_TRUE(frame.spot.valid);
  EXPECT_EQ(frame.spot.position, glm::vec3(1.0f, 2.0f, 3.0f));
  EXPECT_EQ(frame.spot.direction, glm::vec3(0.0f, 0.0f, -1.0f));
  EXPECT_FLOAT_EQ(frame.spot.inner_cutoff_cos, 0.8f);
  EXPECT_FLOAT_EQ(frame.spot.outer_cutoff_cos, 0.6f);
}

TEST(LightFrameTest, FallsBackToMainCameraForSpotLightWhenTargetIsMissing) {
  ecs::World world;

  auto camera = world.spawn("camera");
  camera.emplace<scene::Transform>(scene::Transform{.position = glm::vec3(4.0f, 5.0f, 6.0f)});
  camera.emplace<Camera>(Camera{.front = glm::vec3(1.0f, 0.0f, 0.0f)});
  camera.emplace<MainCamera>();

  auto spot = world.spawn("flashlight");
  spot.emplace<scene::Transform>();
  spot.emplace<Light>(Light{.type = LightType::Spot});

  const auto frame = collect_light_frame(world);

  EXPECT_TRUE(frame.spot.valid);
  EXPECT_EQ(frame.spot.position, glm::vec3(4.0f, 5.0f, 6.0f));
  EXPECT_EQ(frame.spot.direction, glm::vec3(1.0f, 0.0f, 0.0f));
}

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
TEST(LightFrameTest, BuildsForwardLightParamsFromMaterialBindingsAndPreparedLightFrame) {
  ecs::World world;

  auto camera = world.spawn("camera");
  camera.emplace<scene::Transform>(scene::Transform{.position = glm::vec3(1.0f, 2.0f, 3.0f)});
  camera.emplace<Camera>(Camera{.front = glm::vec3(0.0f, 0.0f, -1.0f)});
  camera.emplace<MainCamera>();

  auto sun = world.spawn("sun");
  sun.emplace<scene::Transform>(scene::Transform{.position = glm::vec3(-4.0f, 8.0f, -3.0f)});
  sun.emplace<Light>(Light{.type = LightType::Directional, .color = glm::vec3(1.0f, 0.5f, 0.25f), .intensity = 2.0f});

  auto point = world.spawn("lamp");
  point.emplace<scene::Transform>(scene::Transform{.position = glm::vec3(5.0f, 1.0f, -2.0f)});
  point.emplace<Light>(Light{.type = LightType::Point, .color = glm::vec3(0.5f, 1.0f, 0.25f), .intensity = 3.0f});

  auto spot = world.spawn("flashlight");
  spot.emplace<scene::Transform>();
  spot.emplace<Light>(Light{.type = LightType::Spot});
  spot.emplace<SpotLightTarget>(SpotLightTarget{.camera = camera.id()});

  MaterialBindingState binding;
  binding.diffuse_slot = 2;
  binding.specular_slot = 4;
  binding.normal_map_slot = -1;
  binding.displacement_map_slot = -1;
  binding.shininess = 64.0f;

  const auto frame = collect_light_frame(world);
  auto params = build_forward_light_params(frame, binding, 7);

  EXPECT_EQ(params.materials[0].diffuse, 2);
  EXPECT_EQ(params.materials[0].specular, 4);
  EXPECT_FLOAT_EQ(params.materials[0].shininess, 64.0f);
  EXPECT_EQ(params.normal_map, 2);
  EXPECT_EQ(params.displacement_map, 4);
  EXPECT_EQ(params.shadow_map, 7);
  EXPECT_EQ(params.directional.position, glm::vec3(-4.0f, 8.0f, -3.0f));
  EXPECT_EQ(params.point_lights[0].position, glm::vec3(5.0f, 1.0f, -2.0f));
  EXPECT_EQ(params.spot_light.position, glm::vec3(1.0f, 2.0f, 3.0f));
  EXPECT_EQ(params.spot_light.direction, glm::vec3(0.0f, 0.0f, -1.0f));
}
#endif

} // namespace
} // namespace astralix::rendering
