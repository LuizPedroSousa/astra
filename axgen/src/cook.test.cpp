#include "cook.hpp"

#include "args.hpp"
#include "exceptions/base-exception.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

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

void write_test_project(
    const std::filesystem::path &root,
    bool with_expected_material_slots
) {
  write_text(
      root / "project.ax",
      R"json({
  "project": {
    "resources": { "directory": "assets" },
    "serialization": { "format": "json" }
  },
  "resources": [
    {
      "id": "models::triangle",
      "asset": "models/triangle.axmodel"
    }
  ]
})json"
  );

  write_text(
      root / "assets" / "materials" / "triangle.axmaterial",
      R"json({
  "version": 1,
  "textures": {}
})json"
  );

  write_text(
      root / "assets" / "models" / "triangle.axmodel",
      with_expected_material_slots ? R"json({
  "version": 1,
  "source": "source/triangle.obj",
  "materials": [
    "../materials/triangle.axmaterial",
    "../materials/triangle.axmaterial"
  ]
})json"
                    : R"json({
  "version": 1,
  "source": "source/triangle.obj",
  "materials": []
})json"
  );

  write_text(
      root / "assets" / "models" / "source" / "triangle.obj",
      R"obj(mtllib triangle.mtl
o Triangle
v 0.0 0.0 0.0
v 1.0 0.0 0.0
v 0.0 1.0 0.0
vt 0.0 0.0
vt 1.0 0.0
vt 0.0 1.0
vn 0.0 0.0 1.0
usemtl default
f 1/1/1 2/2/1 3/3/1
)obj"
  );
  write_text(
      root / "assets" / "models" / "source" / "triangle.mtl",
      "newmtl default\nKd 1.0 1.0 1.0\n"
  );
}

TEST(AxgenCook, GeneratesPackManifestAndAxmeshArtifact) {
  const auto root = make_temp_root("astralix-axgen-cook");
  write_test_project(root, true);

  Options options;
  options.command = Options::Command::CookAssets;
  options.root_path = root.string();

  const auto result = run_cook_once(options);
  ASSERT_TRUE(result.ok);

  const auto manifest_path = root / ".astralix" / "cooked" / "pack.axpack";
  ASSERT_TRUE(std::filesystem::exists(manifest_path));

  std::ifstream manifest_stream(manifest_path);
  std::stringstream manifest_buffer;
  manifest_buffer << manifest_stream.rdbuf();
  const auto manifest_text = manifest_buffer.str();
  EXPECT_NE(manifest_text.find("models::triangle"), std::string::npos);
  EXPECT_NE(manifest_text.find(".axmesh"), std::string::npos);

  size_t axmesh_count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(
           root / ".astralix" / "cooked" / "artifacts" / "models")) {
    if (entry.path().extension() == ".axmesh") {
      ++axmesh_count;
    }
  }
  EXPECT_EQ(axmesh_count, 1u);
}

TEST(AxgenCook, RejectsModelWhenMaterialSlotsAreMissing) {
  const auto root = make_temp_root("astralix-axgen-cook-mismatch");
  write_test_project(root, false);

  Options options;
  options.command = Options::Command::CookAssets;
  options.root_path = root.string();

  EXPECT_THROW(run_cook_once(options), astralix::BaseException);
}

} // namespace
} // namespace axgen
