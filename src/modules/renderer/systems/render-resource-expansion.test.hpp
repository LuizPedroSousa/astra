#include "render-resource-expansion.hpp"

#include "components/transform.hpp"
#include <gtest/gtest.h>

namespace astralix::rendering {
namespace {

TEST(RenderResourceExpansionTest, ExpandsRequestIntoCanonicalRenderComponents) {
  ecs::World world;

  auto entity = world.spawn("mesh");
  entity.emplace<scene::Transform>();
  entity.emplace<RenderResourceRequest>(RenderResourceRequest{
      .model_ids = {"models::crate"},
      .shader_id = "shaders::lighting",
      .material_ids = {"materials::stone"},
      .textures = {TextureBinding{
          .id = "textures::albedo",
          .name = "materials[0].diffuse",
      }},
      .renderable = true,
  });

  expand_render_resource_requests(world);

  EXPECT_FALSE(entity.has<RenderResourceRequest>());
  ASSERT_NE(entity.get<scene::Transform>(), nullptr);
  ASSERT_NE(entity.get<ModelRef>(), nullptr);
  EXPECT_EQ(entity.get<ModelRef>()->resource_ids.size(), 1u);
  ASSERT_NE(entity.get<ShaderBinding>(), nullptr);
  EXPECT_EQ(entity.get<ShaderBinding>()->shader, "shaders::lighting");
  ASSERT_NE(entity.get<MaterialSlots>(), nullptr);
  EXPECT_EQ(entity.get<MaterialSlots>()->materials.size(), 1u);
  ASSERT_NE(entity.get<TextureBindings>(), nullptr);
  EXPECT_EQ(entity.get<TextureBindings>()->bindings.size(), 1u);
  EXPECT_TRUE(entity.has<Renderable>());
}

TEST(RenderResourceExpansionTest, CanRemoveRenderableWhenRequestDisablesIt) {
  ecs::World world;

  auto entity = world.spawn("helper");
  entity.emplace<Renderable>();
  entity.emplace<RenderResourceRequest>(RenderResourceRequest{
      .renderable = false,
  });

  expand_render_resource_requests(world);

  EXPECT_FALSE(entity.has<RenderResourceRequest>());
  EXPECT_FALSE(entity.has<Renderable>());
}

} // namespace
} // namespace astralix::rendering
