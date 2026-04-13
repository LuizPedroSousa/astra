#include "systems/render-system/material-binding.hpp"
#include "systems/render-system/render-residency.hpp"
#include "systems/render-system/scene-selection.hpp"

#include "components/ui.hpp"
#include "document/document.hpp"
#include <gtest/gtest.h>

namespace astralix::rendering {
namespace {

TEST(SceneSelectionExtractionTest, ExtractsCopiedCameraSkyboxTextAndUiData) {
  ecs::World world;

  auto camera_entity = world.spawn("camera");
  camera_entity.emplace<scene::Transform>(
      scene::Transform{.position = glm::vec3(1.0f, 2.0f, 3.0f)}
  );
  camera_entity.emplace<Camera>(Camera{
      .up = glm::vec3(0.0f, 1.0f, 0.0f),
      .front = glm::vec3(0.0f, 0.0f, -1.0f),
      .view_matrix = glm::mat4(2.0f),
      .projection_matrix = glm::mat4(3.0f),
      .fov_degrees = 70.0f,
      .orthographic_scale = 22.0f,
      .orthographic = true,
  });
  camera_entity.emplace<MainCamera>();

  auto skybox_entity = world.spawn("skybox");
  skybox_entity.emplace<SkyboxBinding>(
      SkyboxBinding{.cubemap = "cubemaps::day", .shader = "shaders::skybox"}
  );

  auto text_entity = world.spawn("text");
  text_entity.emplace<TextSprite>(TextSprite{
      .text = "visible",
      .font_id = "fonts::mono",
      .position = glm::vec2(10.0f, 20.0f),
      .scale = 1.5f,
      .color = glm::vec3(0.2f, 0.3f, 0.4f),
  });

  auto front_doc = create_ref<ui::UIDocument>();
  front_doc->draw_list().commands.push_back(ui::UIDrawCommand{
      .type = ui::DrawCommandType::Rect,
      .rect = {.x = 4.0f, .y = 5.0f, .width = 6.0f, .height = 7.0f},
  });

  auto back_doc = create_ref<ui::UIDocument>();
  back_doc->draw_list().commands.push_back(ui::UIDrawCommand{
      .type = ui::DrawCommandType::Text,
      .text = "back",
      .font_id = "fonts::ui",
      .font_size = 14.0f,
  });

  auto front_root = world.spawn("front");
  front_root.emplace<UIRoot>(UIRoot{
      .document = front_doc,
      .sort_order = 10,
      .visible = true,
  });

  auto back_root = world.spawn("back");
  back_root.emplace<UIRoot>(UIRoot{
      .document = back_doc,
      .sort_order = 1,
      .visible = true,
  });

  const auto camera = extract_main_camera_frame(world);
  const auto skybox = extract_skybox_frame(world);
  const auto text_items = extract_text_items(world);
  const auto ui_roots = extract_ui_roots(world);

  ASSERT_TRUE(camera.has_value());
  EXPECT_EQ(camera->entity_id, camera_entity.id());
  EXPECT_EQ(camera->position, glm::vec3(1.0f, 2.0f, 3.0f));
  EXPECT_EQ(camera->view, glm::mat4(2.0f));
  EXPECT_TRUE(camera->orthographic);

  ASSERT_TRUE(skybox.has_value());
  EXPECT_EQ(skybox->shader_id, "shaders::skybox");
  EXPECT_EQ(skybox->cubemap_id, "cubemaps::day");

  ASSERT_EQ(text_items.size(), 1u);
  EXPECT_EQ(text_items[0].entity_id, text_entity.id());
  EXPECT_EQ(text_items[0].sprite.text, "visible");

  ASSERT_EQ(ui_roots.size(), 2u);
  EXPECT_EQ(ui_roots[0].entity_id, back_root.id());
  EXPECT_EQ(ui_roots[1].entity_id, front_root.id());
  ASSERT_EQ(ui_roots[1].commands.size(), 1u);
  EXPECT_EQ(ui_roots[1].commands[0].rect.x, 4.0f);

  front_doc->draw_list().commands[0].rect.x = 99.0f;
  EXPECT_EQ(ui_roots[1].commands[0].rect.x, 4.0f);
}

TEST(SceneResidencyRequestsTest, CollectsUiResourcesAndFontSizes) {
  SceneResidencyRequests requests;

  ui::UIDrawCommand image_command{
      .type = ui::DrawCommandType::Image,
      .texture_id = "textures::panel",
  };
  ui::UIDrawCommand svg_command{
      .type = ui::DrawCommandType::SvgImage,
      .texture_id = "svgs::logo",
  };
  ui::UIDrawCommand text_command{
      .type = ui::DrawCommandType::Text,
      .font_id = "fonts::ui",
      .font_size = 17.6f,
  };

  request_ui_command_resources(requests, image_command);
  request_ui_command_resources(requests, svg_command);
  request_ui_command_resources(requests, text_command);

  EXPECT_TRUE(requests.textures_2d.contains("textures::panel"));
  EXPECT_TRUE(requests.svgs.contains("svgs::logo"));
  EXPECT_TRUE(requests.fonts.contains("fonts::ui"));
  ASSERT_TRUE(requests.font_sizes.contains("fonts::ui"));
  EXPECT_TRUE(requests.font_sizes.at("fonts::ui").contains(18u));
}

TEST(MaterialBindingTest, ResolvesFallbackModelMaterialSlots) {
  Model model(ResourceHandle{1, 1}, {}, {"materials::fallback"});
  MaterialSlots fallback_slots;

  const auto *resolved = resolve_material_slots(&model, nullptr, fallback_slots);

  ASSERT_NE(resolved, nullptr);
  ASSERT_EQ(resolved->materials.size(), 1u);
  EXPECT_EQ(resolved->materials[0], "materials::fallback");
}

} // namespace
} // namespace astralix::rendering
