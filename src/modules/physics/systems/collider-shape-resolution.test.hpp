#include "collider-shape-resolution.hpp"

#include <gtest/gtest.h>

namespace astralix {
namespace {

Mesh make_bounds_test_mesh() {
  return Mesh(
      {{.position = glm::vec3(-1.0f, -1.0f, -1.0f),
        .normal = glm::vec3(0.0f, 0.0f, 1.0f),
        .texture_coordinates = glm::vec2(0.0f, 0.0f)},
       {.position = glm::vec3(1.0f, -1.0f, -1.0f),
        .normal = glm::vec3(0.0f, 0.0f, 1.0f),
        .texture_coordinates = glm::vec2(1.0f, 0.0f)},
       {.position = glm::vec3(1.0f, 1.0f, 1.0f),
        .normal = glm::vec3(0.0f, 0.0f, 1.0f),
        .texture_coordinates = glm::vec2(1.0f, 1.0f)},
       {.position = glm::vec3(-1.0f, 1.0f, 1.0f),
        .normal = glm::vec3(0.0f, 0.0f, 1.0f),
        .texture_coordinates = glm::vec2(0.0f, 1.0f)}},
      {0, 1, 2, 2, 3, 0});
}

TEST(ColliderShapeResolutionTest, ComputesBoundsForInlineMeshSets) {
  rendering::MeshSet mesh_set{.meshes = {make_bounds_test_mesh()}};

  auto bounds = compute_mesh_bounds(mesh_set);
  ASSERT_TRUE(bounds.has_value());
  EXPECT_TRUE(bounds->valid);
  EXPECT_FLOAT_EQ(bounds->min.x, -1.0f);
  EXPECT_FLOAT_EQ(bounds->min.y, -1.0f);
  EXPECT_FLOAT_EQ(bounds->min.z, -1.0f);
  EXPECT_FLOAT_EQ(bounds->max.x, 1.0f);
  EXPECT_FLOAT_EQ(bounds->max.y, 1.0f);
  EXPECT_FLOAT_EQ(bounds->max.z, 1.0f);
}

TEST(ColliderShapeResolutionTest, ResolvesColliderFromInlineRenderMesh) {
  ecs::World world;
  auto entity = world.spawn("box");
  entity.emplace<rendering::MeshSet>(
      rendering::MeshSet{.meshes = {make_bounds_test_mesh()}});
  entity.emplace<physics::FitBoxColliderFromRenderMesh>();

  resolve_box_colliders_from_render_mesh(world);

  ASSERT_NE(entity.get<physics::BoxCollider>(), nullptr);
  EXPECT_EQ(entity.get<physics::BoxCollider>()->center, glm::vec3(0.0f));
  EXPECT_EQ(entity.get<physics::BoxCollider>()->half_extents,
            glm::vec3(1.0f));
  EXPECT_FALSE(entity.has<physics::FitBoxColliderFromRenderMesh>());
}

TEST(ColliderShapeResolutionTest,
     LeavesExplicitBoxColliderUntouchedWithoutFitMarker) {
  ecs::World world;
  auto entity = world.spawn("box");
  entity.emplace<physics::BoxCollider>(physics::BoxCollider{
      .half_extents = glm::vec3(2.0f, 3.0f, 4.0f),
      .center = glm::vec3(1.0f, 2.0f, 3.0f),
  });

  resolve_box_colliders_from_render_mesh(world);

  ASSERT_NE(entity.get<physics::BoxCollider>(), nullptr);
  EXPECT_EQ(entity.get<physics::BoxCollider>()->half_extents,
            glm::vec3(2.0f, 3.0f, 4.0f));
  EXPECT_EQ(entity.get<physics::BoxCollider>()->center,
            glm::vec3(1.0f, 2.0f, 3.0f));
}

} // namespace
} // namespace astralix
