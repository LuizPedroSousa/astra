#include "terrain-recipe-data.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace astralix::terrain {
namespace {

std::filesystem::path make_temp_root(const char *suffix) {
  const auto root =
      std::filesystem::temp_directory_path() / std::string(suffix);
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  return root;
}

void write_text(const std::filesystem::path &path, const std::string &text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);
  out << text;
}

TEST(TerrainRecipeDataTest, ParsesRootlessVersionedRecipe) {
  const auto root = make_temp_root("astralix-terrain-rootless");
  const auto recipe_path = root / "rootless.axterrain";

  write_text(
      recipe_path,
      R"json({
  "version": 1,
  "resolution": 513,
  "noise": { "type": "fbm", "seed": 7 },
  "splat": {
    "layers": [
      {
        "material_id": "materials::stone",
        "channel": "g",
        "min_height": 0.1,
        "max_height": 0.9
      }
    ]
  }
})json"
  );

  const auto recipe = parse_terrain_recipe(recipe_path.string());
  EXPECT_EQ(recipe.resolution, 513u);
  EXPECT_EQ(recipe.noise.type, "fbm");
  ASSERT_EQ(recipe.splat.layers.size(), 1u);
  EXPECT_EQ(recipe.splat.layers[0].material_id, "materials::stone");
  EXPECT_EQ(recipe.splat.layers[0].channel, "g");
}

TEST(TerrainRecipeDataTest, ParsesLegacyWrappedRecipe) {
  const auto root = make_temp_root("astralix-terrain-legacy");
  const auto recipe_path = root / "legacy.axterrain";

  write_text(
      recipe_path,
      R"json({
  "terrain": {
    "version": 1,
    "resolution": 257,
    "noise": { "type": "fbm", "seed": 42 }
  }
})json"
  );

  const auto recipe = parse_terrain_recipe(recipe_path.string());
  EXPECT_EQ(recipe.resolution, 257u);
  EXPECT_EQ(recipe.noise.seed, 42u);
}

} // namespace
} // namespace astralix::terrain
