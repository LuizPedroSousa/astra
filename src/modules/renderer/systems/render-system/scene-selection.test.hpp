#include "scene-selection.hpp"

#include <cmath>
#include <gtest/gtest.h>

namespace astralix::rendering {
namespace {

TEST(SceneSelectionTest, SelectsMainCameraBeforeFallbackCamera) {
  ecs::World world;

  auto fallback = world.spawn("fallback");
  fallback.emplace<scene::Transform>();
  fallback.emplace<Camera>();

  auto main = world.spawn("main");
  main.emplace<scene::Transform>(scene::Transform{.position = glm::vec3(1.0f, 2.0f, 3.0f)});
  main.emplace<Camera>();
  main.emplace<MainCamera>();

  auto selected = select_main_camera(world);

  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->entity_id, main.id());
  ASSERT_NE(selected->transform, nullptr);
  EXPECT_EQ(selected->transform->position, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(SceneSelectionTest, SelectsFirstActiveSkybox) {
  ecs::World world;

  auto inactive = world.spawn("inactive");
  inactive.emplace<SkyboxBinding>(SkyboxBinding{
      .cubemap = "cubemaps::inactive",
      .shader = "shaders::skybox",
  });
  inactive.set_active(false);

  auto active = world.spawn("active");
  active.emplace<SkyboxBinding>(SkyboxBinding{
      .cubemap = "cubemaps::active",
      .shader = "shaders::skybox",
  });

  auto selected = select_skybox(world);

  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->entity_id, active.id());
  ASSERT_NE(selected->skybox, nullptr);
  EXPECT_EQ(selected->skybox->cubemap, "cubemaps::active");
}

TEST(SceneSelectionTest, CollectsOnlyActiveTextSprites) {
  ecs::World world;

  auto inactive = world.spawn("inactive");
  inactive.emplace<TextSprite>(TextSprite{.text = "hidden"});
  inactive.set_active(false);

  auto active = world.spawn("active");
  active.emplace<TextSprite>(TextSprite{.text = "visible"});

  auto sprites = collect_text_sprites(world);

  ASSERT_EQ(sprites.size(), 1u);
  EXPECT_EQ(sprites[0].entity_id, active.id());
  ASSERT_NE(sprites[0].sprite, nullptr);
  EXPECT_EQ(sprites[0].sprite->text, "visible");
}

} // namespace
} // namespace astralix::rendering
