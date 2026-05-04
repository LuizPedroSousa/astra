#include "material-binding.hpp"

#include <gtest/gtest.h>

namespace astralix::rendering {
namespace {

TEST(MaterialBindingTest, ResolvesPbrSemanticsFromLayoutNames) {
  MaterialBindingLayout layout{
      .base_color = MaterialTextureBindingPoint{
          .logical_name = "material.base_color",
          .binding_id = 1u,
      },
      .normal = MaterialTextureBindingPoint{
          .logical_name = "material.normal",
          .binding_id = 2u,
      },
      .metallic = MaterialTextureBindingPoint{
          .logical_name = "material.metallic",
          .binding_id = 3u,
      },
      .roughness = MaterialTextureBindingPoint{
          .logical_name = "material.roughness",
          .binding_id = 4u,
      },
      .occlusion = MaterialTextureBindingPoint{
          .logical_name = "material.occlusion",
          .binding_id = 5u,
      },
      .emissive = MaterialTextureBindingPoint{
          .logical_name = "material.emissive",
          .binding_id = 6u,
      },
      .displacement = MaterialTextureBindingPoint{
          .logical_name = "material.displacement",
          .binding_id = 7u,
      },
  };

  auto semantic = resolve_material_binding_semantic(layout, "material.metallic");

  ASSERT_TRUE(semantic.has_value());
  EXPECT_EQ(*semantic, MaterialBindingSemantic::Metallic);
  EXPECT_FALSE(resolve_material_binding_semantic(layout, "material.diffuse")
                   .has_value());
}

TEST(MaterialBindingTest, UpdateMaterialBindingSlotSetsOnlyUnassignedPbrSlots) {
  MaterialBindingState state;
  update_material_binding_slot(state, MaterialBindingSemantic::BaseColor, 2);
  update_material_binding_slot(state, MaterialBindingSemantic::Normal, 3);
  update_material_binding_slot(state, MaterialBindingSemantic::Metallic, 4);
  update_material_binding_slot(state, MaterialBindingSemantic::Roughness, 5);
  update_material_binding_slot(state, MaterialBindingSemantic::Occlusion, 6);
  update_material_binding_slot(state, MaterialBindingSemantic::Emissive, 7);
  update_material_binding_slot(state, MaterialBindingSemantic::Displacement, 8);
  update_material_binding_slot(state, MaterialBindingSemantic::BaseColor, 9);

  EXPECT_EQ(state.base_color_slot, 2);
  EXPECT_EQ(state.normal_slot, 3);
  EXPECT_EQ(state.metallic_slot, 4);
  EXPECT_EQ(state.roughness_slot, 5);
  EXPECT_EQ(state.occlusion_slot, 6);
  EXPECT_EQ(state.emissive_slot, 7);
  EXPECT_EQ(state.displacement_slot, 8);
}

TEST(MaterialBindingTest, ResolvesPerSubmeshMaterialIds) {
  MaterialSlots material_slots{
      .materials = {"materials::a", "materials::b", "materials::c"},
  };

  const auto *first =
      resolve_material_id_for_submesh(nullptr, &material_slots, 0u);
  const auto *second =
      resolve_material_id_for_submesh(nullptr, &material_slots, 1u);
  const auto *overflow =
      resolve_material_id_for_submesh(nullptr, &material_slots, 7u);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(overflow, nullptr);
  EXPECT_EQ(*first, "materials::a");
  EXPECT_EQ(*second, "materials::b");
  EXPECT_EQ(*overflow, "materials::a");
}

TEST(MaterialBindingTest, ResolvesModelMaterialSlotsForSubmeshes) {
  Model model(
      ResourceHandle{1, 1},
      {},
      {"materials::stone", "materials::wood", "materials::metal"},
      {2u, 0u, 1u, 2u}
  );

  const auto *first = resolve_material_id_for_submesh(&model, nullptr, 0u);
  const auto *second = resolve_material_id_for_submesh(&model, nullptr, 1u);
  const auto *third = resolve_material_id_for_submesh(&model, nullptr, 2u);
  const auto *fourth = resolve_material_id_for_submesh(&model, nullptr, 3u);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(third, nullptr);
  ASSERT_NE(fourth, nullptr);
  EXPECT_EQ(*first, "materials::metal");
  EXPECT_EQ(*second, "materials::stone");
  EXPECT_EQ(*third, "materials::wood");
  EXPECT_EQ(*fourth, "materials::metal");
}

TEST(MaterialBindingTest, UsesNeutralFallbackPbrTextureIds) {
  const ResolvedMaterialData material_data{};

  const auto metallic =
      resolve_surface_texture_binding(material_data, MaterialBindingSemantic::Metallic);
  const auto roughness =
      resolve_surface_texture_binding(material_data, MaterialBindingSemantic::Roughness);

  EXPECT_EQ(
      metallic.descriptor_id,
      "textures::defaults::pbr_metallic_black"
  );
  EXPECT_EQ(metallic.channel, 0);
  EXPECT_EQ(
      roughness.descriptor_id,
      "textures::defaults::pbr_roughness_white"
  );
  EXPECT_EQ(roughness.channel, 0);
}

} // namespace
} // namespace astralix::rendering
