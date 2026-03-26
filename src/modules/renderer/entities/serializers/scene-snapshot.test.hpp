#include "scene-snapshot.hpp"

#include <gtest/gtest.h>

namespace astralix::serialization {
namespace {

Mesh make_test_quad_mesh() {
  return Mesh(
      {{.position = glm::vec3(-0.5f, -0.5f, 0.0f),
        .normal = glm::vec3(0.0f, 0.0f, 1.0f),
        .texture_coordinates = glm::vec2(0.0f, 0.0f)},
       {.position = glm::vec3(0.5f, -0.5f, 0.0f),
        .normal = glm::vec3(0.0f, 0.0f, 1.0f),
        .texture_coordinates = glm::vec2(1.0f, 0.0f)},
       {.position = glm::vec3(0.5f, 0.5f, 0.0f),
        .normal = glm::vec3(0.0f, 0.0f, 1.0f),
        .texture_coordinates = glm::vec2(1.0f, 1.0f)},
       {.position = glm::vec3(-0.5f, 0.5f, 0.0f),
        .normal = glm::vec3(0.0f, 0.0f, 1.0f),
        .texture_coordinates = glm::vec2(0.0f, 1.0f)}},
      {0, 1, 2, 2, 3, 0});
}

TEST(SceneSnapshotTest, CollectsOnlySceneEntitiesWithKnownComponents) {
  ecs::World world;

  auto scene_entity = world.spawn("camera");
  scene_entity.emplace<scene::SceneEntity>();
  scene_entity.emplace<scene::Transform>();
  scene_entity.emplace<rendering::Camera>();
  scene_entity.emplace<rendering::MainCamera>();

  auto hidden_entity = world.spawn("helper");
  hidden_entity.emplace<scene::Transform>();

  auto snapshots = collect_scene_snapshots(world);

  ASSERT_EQ(snapshots.size(), 1u);
  EXPECT_EQ(snapshots[0].name, "camera");
  EXPECT_TRUE(snapshots[0].active);
  EXPECT_FALSE(snapshots[0].components.empty());

  bool found_transform = false;
  bool found_camera = false;
  for (const auto &component : snapshots[0].components) {
    if (component.name == "Transform") {
      found_transform = true;
    }
    if (component.name == "Camera") {
      found_camera = true;
    }
  }

  EXPECT_TRUE(found_transform);
  EXPECT_TRUE(found_camera);
}

TEST(SceneSnapshotTest, FlattensVectorAndBindingFields) {
  ecs::World world;
  auto entity = world.spawn("mesh");
  entity.emplace<scene::SceneEntity>();
  entity.emplace<rendering::ModelRef>(
      rendering::ModelRef{.resource_ids = {"models::cube", "models::sphere"}});
  entity.emplace<rendering::MaterialSlots>(
      rendering::MaterialSlots{.materials = {"materials::stone", "materials::metal"}});
  entity.emplace<rendering::TextureBindings>(rendering::TextureBindings{
      .bindings = {rendering::TextureBinding{.id = "textures::albedo",
                                             .name = "light.albedo",
                                             .cubemap = false}}});

  auto snapshots = collect_scene_snapshots(world);
  ASSERT_EQ(snapshots.size(), 1u);

  bool found_mesh = false;
  bool found_textures = false;
  for (const auto &component : snapshots[0].components) {
    if (component.name == "ModelRef") {
      found_mesh = component.fields.size() == 2u;
    }

    if (component.name == "TextureBindings") {
      found_textures = component.fields.size() == 3u;
    }
  }

  EXPECT_TRUE(found_mesh);
  EXPECT_TRUE(found_textures);
}

TEST(SceneSnapshotTest,
     RoundTripsMeshSetSkyboxTextLightAndPhysicsComponents) {
  ecs::World source;

  auto camera = source.spawn("camera");
  camera.emplace<scene::SceneEntity>();
  camera.emplace<scene::Transform>();
  camera.emplace<rendering::Camera>();
  camera.emplace<rendering::MainCamera>();

  auto entity = source.spawn("crate");
  entity.emplace<scene::SceneEntity>();
  entity.emplace<rendering::Renderable>();
  entity.emplace<rendering::ShadowCaster>();
  entity.emplace<scene::Transform>(scene::Transform{
      .position = glm::vec3(1.0f, 2.0f, 3.0f),
      .scale = glm::vec3(2.0f),
      .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
      .matrix = glm::mat4(1.0f),
      .dirty = false,
  });
  entity.emplace<rendering::MeshSet>(rendering::MeshSet{.meshes = {make_test_quad_mesh()}});
  entity.emplace<rendering::SkyboxBinding>(rendering::SkyboxBinding{
      .cubemap = "cubemaps::skybox",
      .shader = "shaders::skybox",
  });
  entity.emplace<rendering::TextSprite>(rendering::TextSprite{
      .text = "hello",
      .font_id = "fonts::roboto",
      .position = glm::vec2(32.0f, 64.0f),
      .scale = 1.25f,
      .color = glm::vec3(0.4f, 0.5f, 0.6f),
  });
  entity.emplace<physics::RigidBody>(physics::RigidBody{
      .mode = physics::RigidBodyMode::Dynamic,
      .gravity = 0.5f,
      .velocity = 3.0f,
      .acceleration = 4.0f,
      .drag = 0.25f,
      .mass = 5.0f,
  });
  entity.emplace<physics::BoxCollider>(physics::BoxCollider{
      .half_extents = glm::vec3(0.75f, 1.5f, 0.5f),
      .center = glm::vec3(0.1f, 0.2f, 0.3f),
  });
  entity.emplace<physics::FitBoxColliderFromRenderMesh>();

  auto point_light = source.spawn("lamp");
  point_light.emplace<scene::SceneEntity>();
  point_light.emplace<scene::Transform>(scene::Transform{.position = glm::vec3(8.0f, 2.0f, 1.0f)});
  point_light.emplace<rendering::Light>(rendering::Light{
      .type = rendering::LightType::Point,
      .color = glm::vec3(0.2f, 0.3f, 0.4f),
      .intensity = 2.5f,
      .casts_shadows = false,
  });
  point_light.emplace<rendering::PointLightAttenuation>(rendering::PointLightAttenuation{
      .constant = 2.0f,
      .linear = 0.1f,
      .quadratic = 0.01f,
  });

  auto spot_light = source.spawn("flashlight");
  spot_light.emplace<scene::SceneEntity>();
  spot_light.emplace<scene::Transform>();
  spot_light.emplace<rendering::Light>(
      rendering::Light{.type = rendering::LightType::Spot});
  spot_light.emplace<rendering::SpotLightTarget>(rendering::SpotLightTarget{.camera = camera.id()});
  spot_light.emplace<rendering::SpotLightCone>(rendering::SpotLightCone{
      .inner_cutoff_cos = 0.8f,
      .outer_cutoff_cos = 0.6f,
  });

  auto sun = source.spawn("sun");
  sun.emplace<scene::SceneEntity>();
  sun.emplace<scene::Transform>(scene::Transform{.position = glm::vec3(-4.0f, 8.0f, -3.0f)});
  sun.emplace<rendering::Light>(
      rendering::Light{.type = rendering::LightType::Directional});
  sun.emplace<rendering::DirectionalShadowSettings>(rendering::DirectionalShadowSettings{
      .ortho_extent = 15.0f,
      .near_plane = 0.5f,
      .far_plane = 150.0f,
  });

  auto snapshots = collect_scene_snapshots(source);
  ASSERT_EQ(snapshots.size(), 5u);

  ecs::World restored;
  apply_scene_snapshots(restored, snapshots);

  auto restored_entity = restored.entity(entity.id());
  ASSERT_TRUE(restored_entity.exists());
  EXPECT_TRUE(restored_entity.has<rendering::MeshSet>());
  EXPECT_TRUE(restored_entity.has<rendering::SkyboxBinding>());
  EXPECT_TRUE(restored_entity.has<rendering::TextSprite>());
  EXPECT_TRUE(restored_entity.has<physics::RigidBody>());
  EXPECT_TRUE(restored_entity.has<physics::BoxCollider>());
  EXPECT_TRUE(restored_entity.has<physics::FitBoxColliderFromRenderMesh>());

  auto *mesh_set = restored_entity.get<rendering::MeshSet>();
  ASSERT_NE(mesh_set, nullptr);
  ASSERT_EQ(mesh_set->meshes.size(), 1u);
  EXPECT_FALSE(mesh_set->meshes[0].vertices.empty());
  EXPECT_FALSE(mesh_set->meshes[0].indices.empty());

  auto *sprite = restored_entity.get<rendering::TextSprite>();
  ASSERT_NE(sprite, nullptr);
  EXPECT_EQ(sprite->text, "hello");
  EXPECT_EQ(sprite->font_id, "fonts::roboto");

  auto *rigid_body = restored_entity.get<physics::RigidBody>();
  ASSERT_NE(rigid_body, nullptr);
  EXPECT_EQ(rigid_body->mode, physics::RigidBodyMode::Dynamic);
  EXPECT_FLOAT_EQ(rigid_body->mass, 5.0f);

  auto *collider = restored_entity.get<physics::BoxCollider>();
  ASSERT_NE(collider, nullptr);
  EXPECT_EQ(collider->center, glm::vec3(0.1f, 0.2f, 0.3f));

  auto restored_point_light = restored.entity(point_light.id());
  ASSERT_TRUE(restored_point_light.exists());
  ASSERT_NE(restored_point_light.get<rendering::Light>(), nullptr);
  EXPECT_EQ(restored_point_light.get<rendering::Light>()->type,
            rendering::LightType::Point);
  ASSERT_NE(restored_point_light.get<rendering::PointLightAttenuation>(), nullptr);
  EXPECT_FLOAT_EQ(restored_point_light.get<rendering::PointLightAttenuation>()->linear,
                  0.1f);

  auto restored_spot_light = restored.entity(spot_light.id());
  ASSERT_TRUE(restored_spot_light.exists());
  ASSERT_NE(restored_spot_light.get<rendering::SpotLightTarget>(), nullptr);
  ASSERT_TRUE(restored_spot_light.get<rendering::SpotLightTarget>()->camera.has_value());
  EXPECT_EQ(*restored_spot_light.get<rendering::SpotLightTarget>()->camera, camera.id());
  ASSERT_NE(restored_spot_light.get<rendering::SpotLightCone>(), nullptr);
  EXPECT_FLOAT_EQ(restored_spot_light.get<rendering::SpotLightCone>()->inner_cutoff_cos,
                  0.8f);

  auto restored_sun = restored.entity(sun.id());
  ASSERT_TRUE(restored_sun.exists());
  ASSERT_NE(restored_sun.get<rendering::DirectionalShadowSettings>(), nullptr);
  EXPECT_FLOAT_EQ(
      restored_sun.get<rendering::DirectionalShadowSettings>()->far_plane, 150.0f);

  auto *transform = restored_entity.get<scene::Transform>();
  ASSERT_NE(transform, nullptr);
  EXPECT_TRUE(transform->dirty);
}

} // namespace
} // namespace astralix::serialization
