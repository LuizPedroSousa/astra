#include "project-serializer.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace axgen {
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

TEST(ProjectSerializerTest, ParsesAssetBindingsAlongsideShaders) {
  const auto root = make_temp_root("astralix-axgen-project-serializer");
  const auto manifest_path = root / "project.ax";

  write_text(
      manifest_path,
      R"json({
  "project": {
    "resources": { "directory": "assets" },
    "serialization": { "format": "json" }
  },
  "resources": [
    {
      "id": "materials::brick",
      "asset": "materials/brick.axmaterial"
    },
    {
      "id": "shaders::main",
      "type": "Shader",
      "vertex": "@engine/shaders/post-process.axsl",
      "fragment": "shaders/local.axsl"
    }
  ]
})json"
  );

  std::string error;
  const auto manifest = ProjectSerializer::deserialize(manifest_path, &error);
  ASSERT_TRUE(manifest.has_value()) << error;
  ASSERT_EQ(manifest->asset_bindings.size(), 1u);
  EXPECT_EQ(manifest->asset_bindings[0].id, "materials::brick");
  EXPECT_EQ(manifest->asset_bindings[0].asset_path, "materials/brick.axmaterial");
  ASSERT_EQ(manifest->shaders.size(), 1u);
  ASSERT_TRUE(manifest->shaders[0].vertex_path.has_value());
  ASSERT_TRUE(manifest->shaders[0].fragment_path.has_value());
}

TEST(ProjectSerializerTest, ParsesComputeOnlyShaders) {
  const auto root = make_temp_root("astralix-axgen-project-serializer-compute");
  const auto manifest_path = root / "project.ax";

  write_text(
      manifest_path,
      R"json({
  "project": {
    "resources": { "directory": "assets" },
    "serialization": { "format": "json" }
  },
  "resources": [
    {
      "id": "shaders::histogram",
      "type": "Shader",
      "compute": "@engine/shaders/eye-adaptation-histogram.axsl"
    }
  ]
})json"
  );

  std::string error;
  const auto manifest = ProjectSerializer::deserialize(manifest_path, &error);
  ASSERT_TRUE(manifest.has_value()) << error;
  ASSERT_EQ(manifest->shaders.size(), 1u);
  EXPECT_FALSE(manifest->shaders[0].vertex_path.has_value());
  EXPECT_FALSE(manifest->shaders[0].fragment_path.has_value());
  ASSERT_TRUE(manifest->shaders[0].compute_path.has_value());
}

TEST(ProjectSerializerTest, RejectsMixedInlineAndAssetEntries) {
  const auto root = make_temp_root("astralix-axgen-project-serializer-mixed");
  const auto manifest_path = root / "project.ax";

  write_text(
      manifest_path,
      R"json({
  "project": {
    "resources": { "directory": "assets" },
    "serialization": { "format": "json" }
  },
  "resources": [
    {
      "id": "materials::brick",
      "type": "Material",
      "asset": "materials/brick.axmaterial"
    }
  ]
})json"
  );

  std::string error;
  const auto manifest = ProjectSerializer::deserialize(manifest_path, &error);
  EXPECT_FALSE(manifest.has_value());
  EXPECT_NE(error.find("mixes inline fields with an asset binding"),
            std::string::npos);
}

} // namespace
} // namespace axgen
