#include "resources/svg-compiler.hpp"

#include <gtest/gtest.h>

#include <filesystem>

namespace astralix {
namespace {

std::filesystem::path project_root_from_this_file() {
  std::filesystem::path root = std::filesystem::path(__FILE__).parent_path();
  for (int index = 0; index < 4; ++index) {
    root = root.parent_path();
  }
  return root;
}

TEST(SvgCompilerTest, CompilesSimpleFilledTrianglePath) {
  const auto svg = R"svg(
<svg width="10" height="10" viewBox="0 0 10 10">
  <path d="M0 0 L10 0 L10 10 Z" fill="#ff0000" />
</svg>
)svg";

  const SvgDocumentData document = compile_svg_string(svg);

  EXPECT_FLOAT_EQ(document.width, 10.0f);
  EXPECT_FLOAT_EQ(document.height, 10.0f);
  ASSERT_EQ(document.batches.size(), 1u);
  ASSERT_EQ(document.batches.front().vertices.size(), 3u);
  EXPECT_NEAR(document.batches.front().vertices.front().color.r, 1.0f, 1.0e-5f);
  EXPECT_NEAR(document.batches.front().vertices.front().color.g, 0.0f, 1.0e-5f);
  EXPECT_NEAR(document.batches.front().vertices.front().color.b, 0.0f, 1.0e-5f);
}

TEST(SvgCompilerTest, AppliesViewBoxScalingToCompiledGeometry) {
  const auto svg = R"svg(
<svg width="32" height="16" viewBox="0 0 8 4">
  <rect x="0" y="0" width="8" height="4" fill="#00ff00" />
</svg>
)svg";

  const SvgDocumentData document = compile_svg_string(svg);

  ASSERT_EQ(document.batches.size(), 1u);
  float max_x = 0.0f;
  float max_y = 0.0f;
  for (const SvgColorVertex &vertex : document.batches.front().vertices) {
    max_x = std::max(max_x, vertex.position.x);
    max_y = std::max(max_y, vertex.position.y);
  }

  EXPECT_NEAR(max_x, 32.0f, 1.0e-4f);
  EXPECT_NEAR(max_y, 16.0f, 1.0e-4f);
}

TEST(SvgCompilerTest, EmitsStrokeGeometryForLines) {
  const auto svg = R"svg(
<svg width="24" height="24" viewBox="0 0 24 24">
  <line x1="2" y1="12" x2="22" y2="12" stroke="#ffffff" stroke-width="4" />
</svg>
)svg";

  const SvgDocumentData document = compile_svg_string(svg);

  ASSERT_EQ(document.batches.size(), 1u);
  EXPECT_GE(document.batches.front().vertices.size(), 6u);
}

TEST(SvgCompilerTest, RejectsUnsupportedFeatureNodes) {
  const auto svg = R"svg(
<svg width="24" height="24" viewBox="0 0 24 24">
  <text x="0" y="10">Nope</text>
</svg>
)svg";

  EXPECT_THROW((void)compile_svg_string(svg), BaseException);
}

TEST(SvgCompilerTest, CompilesEngineIconSvgAssets) {
  const std::filesystem::path icons_root =
      project_root_from_this_file() / "src/assets/icons";
  const std::vector<std::string> icons = {
      "adjust.svg",
      "camera.svg",
      "close.svg",
      "cube.svg",
      "directory.svg",
      "error.svg",
      "file.svg",
      "gizmo-rotate.svg",
      "gizmo-scale.svg",
      "gizmo-translate.svg",
      "info.svg",
      "light.svg",
      "mesh.svg",
      "right-arrow-down.svg",
      "right-arrow.svg",
      "rigidbody.svg",
      "skybox.svg",
      "transform.svg",
      "trash.svg",
      "warn.svg",
  };

  for (const std::string &icon : icons) {
    const std::filesystem::path path = icons_root / icon;
    SCOPED_TRACE(path.string());
    const SvgDocumentData document = compile_svg_file(path);
    EXPECT_GT(document.width, 0.0f);
    EXPECT_GT(document.height, 0.0f);
    EXPECT_FALSE(document.batches.empty());
  }
}

} // namespace
} // namespace astralix
