#include "assets/asset_graph.hpp"
#include "exceptions/base-exception.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace astralix {
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

TEST(AssetGraphTest, ResolvesProjectAndEngineAssetDependencies) {
  const auto root = make_temp_root("astralix-asset-graph-resolve");
  const auto project_root = root / "project";
  const auto resources_root = project_root / "assets";
  const auto engine_root = root / "engine";

  write_text(resources_root / "textures" / "local.png", "png");
  write_text(resources_root / "shared" / "project-normal.png", "png");
  write_text(engine_root / "shared" / "engine-emissive.png", "png");

  write_text(
      resources_root / "textures" / "local.axtexture",
      R"json({
  "version": 1,
  "source": "local.png",
  "flip": false
})json"
  );
  write_text(
      resources_root / "shared" / "project-normal.axtexture",
      R"json({
  "version": 1,
  "source": "project-normal.png",
  "flip": false
})json"
  );
  write_text(
      engine_root / "shared" / "engine-emissive.axtexture",
      R"json({
  "version": 1,
  "source": "engine-emissive.png",
  "flip": false
})json"
  );
  write_text(
      resources_root / "materials" / "test.axmaterial",
      R"json({
  "version": 1,
  "textures": {
    "base_color": "../textures/local.axtexture",
    "normal": "@project/shared/project-normal.axtexture",
    "emissive": "@engine/shared/engine-emissive.axtexture"
  }
})json"
  );

  AssetGraph graph({
      .project_root = project_root,
      .project_resources_root = resources_root,
      .engine_assets_root = engine_root,
  });

  std::vector<AssetBindingConfig> roots = {
      {.id = "materials::test", .asset_path = "materials/test.axmaterial"},
      {.id = "textures::project_normal",
       .asset_path = "shared/project-normal.axtexture"},
  };

  graph.load_root_assets(roots);

  ASSERT_EQ(graph.records().size(), 4u);
  ASSERT_EQ(graph.topological_order().size(), 4u);

  const auto *material = graph.find_by_public_id("materials::test");
  ASSERT_NE(material, nullptr);
  EXPECT_EQ(material->descriptor_id, "materials::test");
  ASSERT_EQ(material->dependencies.size(), 3u);

  const auto *project_normal =
      graph.find_by_public_id("textures::project_normal");
  ASSERT_NE(project_normal, nullptr);
  EXPECT_EQ(project_normal->descriptor_id, "textures::project_normal");

  EXPECT_EQ(material->dependencies[0].descriptor_id,
            "asset::project/textures/local.axtexture");
  EXPECT_EQ(material->dependencies[1].descriptor_id,
            "textures::project_normal");
  EXPECT_EQ(material->dependencies[2].descriptor_id,
            "asset::engine/shared/engine-emissive.axtexture");
}

TEST(AssetGraphTest, RejectsMultiplePublicIdsForSameAsset) {
  const auto root = make_temp_root("astralix-asset-graph-duplicates");
  const auto project_root = root / "project";
  const auto resources_root = project_root / "assets";

  write_text(resources_root / "textures" / "local.png", "png");
  write_text(
      resources_root / "textures" / "local.axtexture",
      R"json({
  "version": 1,
  "source": "local.png",
  "flip": false
})json"
  );

  AssetGraph graph({
      .project_root = project_root,
      .project_resources_root = resources_root,
      .engine_assets_root = root / "engine",
  });

  std::vector<AssetBindingConfig> roots = {
      {.id = "textures::one", .asset_path = "textures/local.axtexture"},
      {.id = "textures::two", .asset_path = "textures/local.axtexture"},
  };

  EXPECT_THROW(graph.load_root_assets(roots), BaseException);
}

} // namespace
} // namespace astralix
