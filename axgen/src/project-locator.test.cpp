#include "project-locator.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace axgen {

TEST(ProjectLocator, FindsManifestBySearchingUpward) {
  const auto root =
      std::filesystem::temp_directory_path() / "astralix-axgen-locator";
  const auto nested = root / "src" / "systems" / "render";
  std::filesystem::create_directories(nested);
  std::ofstream(root / "project.ax") << "{}";

  std::string error;
  auto manifest = find_project_manifest(nested, &error);
  ASSERT_TRUE(manifest.has_value()) << error;
  EXPECT_EQ(*manifest, (root / "project.ax").lexically_normal());

  std::filesystem::remove_all(root);
}

TEST(ProjectLocator, FindsManifestBySearchingDownward) {
  const auto root =
      std::filesystem::temp_directory_path() / "astralix-axgen-downward";
  const auto nested = root / "src";
  std::filesystem::create_directories(nested);
  std::ofstream(nested / "project.ax") << "{}";

  std::string error;
  auto manifest = find_project_manifest(root, &error);
  ASSERT_TRUE(manifest.has_value()) << error;
  EXPECT_EQ(*manifest, (nested / "project.ax").lexically_normal());

  std::filesystem::remove_all(root);
}

TEST(ProjectLocator, SkipsGitignoreDirectories) {
  const auto root =
      std::filesystem::temp_directory_path() / "astralix-axgen-gitignore";
  std::filesystem::create_directories(root / "build" / "output");
  std::filesystem::create_directories(root / "src");

  // Put project.ax in the ignored build dir and the real one in src
  std::ofstream(root / "build" / "output" / "project.ax") << "{}";
  std::ofstream(root / "src" / "project.ax") << "{}";
  std::ofstream(root / ".gitignore") << "build\n";

  std::string error;
  auto manifest = find_project_manifest(root, &error);
  ASSERT_TRUE(manifest.has_value()) << error;
  EXPECT_EQ(*manifest, (root / "src" / "project.ax").lexically_normal());

  std::filesystem::remove_all(root);
}

TEST(ProjectLocator, SkipsHiddenDirectories) {
  const auto root =
      std::filesystem::temp_directory_path() / "astralix-axgen-hidden";
  std::filesystem::create_directories(root / ".cache" / "data");
  std::filesystem::create_directories(root / "app");

  std::ofstream(root / ".cache" / "data" / "project.ax") << "{}";
  std::ofstream(root / "app" / "project.ax") << "{}";

  std::string error;
  auto manifest = find_project_manifest(root, &error);
  ASSERT_TRUE(manifest.has_value()) << error;
  EXPECT_EQ(*manifest, (root / "app" / "project.ax").lexically_normal());

  std::filesystem::remove_all(root);
}

TEST(ProjectLocator, ResolvesExplicitRelativeManifest) {
  const auto root =
      std::filesystem::temp_directory_path() / "astralix-axgen-explicit";
  std::filesystem::create_directories(root / "examples");
  std::ofstream(root / "examples" / "project.ax") << "{}";

  std::string error;
  auto manifest =
      resolve_manifest_path(std::filesystem::path("examples/project.ax"), root, &error);
  ASSERT_TRUE(manifest.has_value()) << error;
  EXPECT_EQ(*manifest, (root / "examples" / "project.ax").lexically_normal());

  std::filesystem::remove_all(root);
}

TEST(ProjectLocator, ReportsMissingManifestDiscovery) {
  const auto root =
      std::filesystem::temp_directory_path() / "astralix-axgen-missing";
  std::filesystem::create_directories(root);

  std::string error;
  auto manifest = find_project_manifest(root, &error);
  EXPECT_FALSE(manifest.has_value());
  EXPECT_NE(error.find("Try '--manifest <path>'"), std::string::npos);

  std::filesystem::remove_all(root);
}

} // namespace axgen
