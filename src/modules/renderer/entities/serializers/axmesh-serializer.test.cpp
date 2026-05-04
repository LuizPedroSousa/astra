#include "axmesh-serializer.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace astralix {
namespace {

std::filesystem::path make_temp_root(const char *suffix) {
  const auto root =
      std::filesystem::temp_directory_path() / std::string(suffix);
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  return root;
}

Mesh make_test_mesh() {
  std::vector<Vertex> vertices = {
      {glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
       glm::vec2(0.0f, 0.0f)},
      {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
       glm::vec2(1.0f, 0.0f)},
      {glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
       glm::vec2(0.0f, 1.0f)},
  };

  return Mesh(std::move(vertices), {0, 1, 2});
}

void write_text(const std::filesystem::path &path, const std::string &text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);
  out << text;
}

TEST(AxMeshSerializerTest, WritesRootlessFormatAndReadsLegacyWrappedFormat) {
  const auto root = make_temp_root("astralix-axmesh-serializer");
  const auto rootless_path = root / "rootless.axmesh";
  const auto legacy_path = root / "legacy.axmesh";

  AxMeshSerializer::write(rootless_path, {make_test_mesh()});

  std::ifstream rootless_stream(rootless_path);
  std::stringstream rootless_buffer;
  rootless_buffer << rootless_stream.rdbuf();
  const auto rootless_text = rootless_buffer.str();

  EXPECT_EQ(rootless_text.find("\"axmesh\""), std::string::npos);
  EXPECT_NE(rootless_text.find("\"version\""), std::string::npos);

  const auto rootless_meshes = AxMeshSerializer::read(rootless_path);
  ASSERT_EQ(rootless_meshes.size(), 1u);
  EXPECT_EQ(rootless_meshes[0].indices.size(), 3u);

  write_text(
      legacy_path,
      R"json({
  "axmesh": {
    "version": 1,
    "meshes": [
      {
        "draw_type": 0,
        "vertices": [
          {
            "position": { "x": 0.0, "y": 0.0, "z": 0.0 },
            "normal": { "x": 0.0, "y": 0.0, "z": 1.0 },
            "texture_coordinates": { "x": 0.0, "y": 0.0 },
            "tangent": { "x": 1.0, "y": 0.0, "z": 0.0 },
            "bitangent_sign": 1.0
          },
          {
            "position": { "x": 1.0, "y": 0.0, "z": 0.0 },
            "normal": { "x": 0.0, "y": 0.0, "z": 1.0 },
            "texture_coordinates": { "x": 1.0, "y": 0.0 },
            "tangent": { "x": 1.0, "y": 0.0, "z": 0.0 },
            "bitangent_sign": 1.0
          },
          {
            "position": { "x": 0.0, "y": 1.0, "z": 0.0 },
            "normal": { "x": 0.0, "y": 0.0, "z": 1.0 },
            "texture_coordinates": { "x": 0.0, "y": 1.0 },
            "tangent": { "x": 1.0, "y": 0.0, "z": 0.0 },
            "bitangent_sign": 1.0
          }
        ],
        "indices": [0, 1, 2]
      }
    ]
  }
})json"
  );

  const auto legacy_meshes = AxMeshSerializer::read(legacy_path);
  ASSERT_EQ(legacy_meshes.size(), 1u);
  EXPECT_EQ(legacy_meshes[0].indices.size(), 3u);
}

} // namespace
} // namespace astralix
