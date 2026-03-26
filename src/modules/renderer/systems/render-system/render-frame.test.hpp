#include "render-frame.hpp"

#include <gtest/gtest.h>

namespace astralix::rendering {
namespace {

TEST(RenderFrameTest, GroupsEntitiesWithSameBatchKey) {
  ecs::World world;
  RenderRuntimeStore runtime_store;

  auto first = world.spawn("first");
  first.emplace<Renderable>();
  first.emplace<scene::Transform>();
  first.emplace<ModelRef>(ModelRef{.resource_ids = {"models::cube"}});
  first.emplace<ShaderBinding>(ShaderBinding{.shader = "shaders::lighting"});
  first.emplace<MaterialSlots>(MaterialSlots{.materials = {"materials::stone"}});

  auto second = world.spawn("second");
  second.emplace<Renderable>();
  second.emplace<scene::Transform>();
  second.emplace<ModelRef>(ModelRef{.resource_ids = {"models::cube"}});
  second.emplace<ShaderBinding>(ShaderBinding{.shader = "shaders::lighting"});
  second.emplace<MaterialSlots>(MaterialSlots{.materials = {"materials::stone"}});

  auto frame = collect_render_frame(world, runtime_store);

  ASSERT_EQ(frame.packets.size(), 2u);
  ASSERT_EQ(frame.batches.size(), 1u);
  EXPECT_EQ(frame.batches[0].entities.size(), 2u);
  EXPECT_TRUE(frame.packets[0].dirty);
  EXPECT_TRUE(frame.packets[1].dirty);
}

TEST(RenderFrameTest, KeepsPacketsCleanUntilInputsChange) {
  ecs::World world;
  RenderRuntimeStore runtime_store;

  auto entity = world.spawn("renderable");
  entity.emplace<Renderable>();
  entity.emplace<scene::Transform>();
  entity.emplace<ModelRef>(ModelRef{.resource_ids = {"models::cube"}});
  entity.emplace<ShaderBinding>(ShaderBinding{.shader = "shaders::lighting"});

  auto first = collect_render_frame(world, runtime_store);
  auto second = collect_render_frame(world, runtime_store);

  ASSERT_EQ(first.packets.size(), 1u);
  ASSERT_EQ(second.packets.size(), 1u);
  EXPECT_TRUE(first.packets[0].dirty);
  EXPECT_FALSE(second.packets[0].dirty);

  entity.get<scene::Transform>()->matrix[3][0] = 5.0f;

  auto third = collect_render_frame(world, runtime_store);
  ASSERT_EQ(third.packets.size(), 1u);
  EXPECT_TRUE(third.packets[0].dirty);
}

TEST(RenderFrameTest, CollectsDirectMeshPacketsForBridgedMeshes) {
  ecs::World world;
  RenderRuntimeStore runtime_store;

  Mesh mesh(
      {
          Vertex{.position = glm::vec3(-1.0f, -1.0f, 0.0f),
                 .normal = glm::vec3(0.0f, 0.0f, 1.0f),
                 .texture_coordinates = glm::vec2(0.0f, 0.0f)},
          Vertex{.position = glm::vec3(1.0f, -1.0f, 0.0f),
                 .normal = glm::vec3(0.0f, 0.0f, 1.0f),
                 .texture_coordinates = glm::vec2(1.0f, 0.0f)},
          Vertex{.position = glm::vec3(0.0f, 1.0f, 0.0f),
                 .normal = glm::vec3(0.0f, 0.0f, 1.0f),
                 .texture_coordinates = glm::vec2(0.5f, 1.0f)},
      },
      {0, 1, 2});

  auto entity = world.spawn("bridged");
  entity.emplace<Renderable>();
  entity.emplace<scene::Transform>();
  entity.emplace<MeshSet>(MeshSet{.meshes = {mesh}});
  entity.emplace<ShaderBinding>(ShaderBinding{.shader = "shaders::lighting"});

  auto frame = collect_render_frame(world, runtime_store);

  ASSERT_EQ(frame.packets.size(), 1u);
  EXPECT_EQ(frame.packets[0].model_ref, nullptr);
  ASSERT_NE(frame.packets[0].mesh_set, nullptr);
  EXPECT_EQ(frame.packets[0].mesh_set->meshes.size(), 1u);
}

} // namespace
} // namespace astralix::rendering
