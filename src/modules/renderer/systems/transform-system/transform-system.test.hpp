#include "transform-system.hpp"

#include <gtest/gtest.h>

namespace astralix::scene {
namespace {

TEST(TransformSystemTest, RecalculatesDirtyMatricesAndClearsDirtyFlag) {
  scene::Transform transform{};
  transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
  transform.scale = glm::vec3(2.0f);
  transform.rotation = glm::angleAxis(glm::radians(90.0f),
                                      glm::vec3(0.0f, 1.0f, 0.0f));
  transform.dirty = true;

  recalculate_transform(transform);

  EXPECT_FALSE(transform.dirty);
  EXPECT_NE(transform.matrix[3][0], 0.0f);
  EXPECT_NE(transform.matrix[3][1], 0.0f);
  EXPECT_NE(transform.matrix[3][2], 0.0f);
}

TEST(TransformSystemTest, UpdateTransformsSkipsCleanTransforms) {
  ecs::World world;
  auto entity = world.spawn("static");
  entity.emplace<scene::Transform>(scene::Transform{
      .position = glm::vec3(4.0f, 0.0f, 0.0f),
      .scale = glm::vec3(1.0f),
      .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
      .matrix = glm::mat4(2.0f),
      .dirty = false,
  });

  update_transforms(world);

  ASSERT_NE(entity.get<scene::Transform>(), nullptr);
  EXPECT_EQ(entity.get<scene::Transform>()->matrix[0][0], 2.0f);
}

} // namespace
} // namespace astralix::scene
