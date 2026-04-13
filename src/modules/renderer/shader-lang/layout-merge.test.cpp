#include "shader-lang/compiler.hpp"
#include "shader-lang/diagnostics.hpp"
#include "shader-lang/layout-merge.hpp"
#include <gtest/gtest.h>

namespace astralix {
namespace {

static std::string errors_str(const CompileResult &result) {
  std::string out;
  for (const auto &error : result.errors) {
    out += error;
    out += '\n';
  }
  return out;
}

TEST(LayoutMergeTest, UniformInterfaceFieldSetAnnotationsPropagateToValueBlock) {
  Compiler compiler;
  static constexpr std::string_view source = R"axsl(
@version 450;

@uniform
interface Camera {
  @set(2) mat4 projection;
}

interface FragmentOutput {
  @location(0) vec4 color;
}

@fragment
fn main(Camera camera) -> FragmentOutput {
  return FragmentOutput(vec4(camera.projection[0][0]));
}
)axsl";

  auto result = compiler.compile(source, "", "camera-set.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_EQ(result.merged_layout.resource_layout.value_blocks.size(), 1u);
  const auto &block = result.merged_layout.resource_layout.value_blocks[0];
  EXPECT_EQ(block.logical_name, "camera");
  ASSERT_TRUE(block.descriptor_set.has_value());
  EXPECT_EQ(*block.descriptor_set, 2u);
}

TEST(LayoutMergeTest, LooseUniformSetAnnotationsPropagateToGlobalsBlock) {
  Compiler compiler;
  static constexpr std::string_view source = R"axsl(
@version 450;

@set(3) uniform float exposure = 1.0;

interface FragmentOutput {
  @location(0) vec4 color;
}

@fragment
fn main() -> FragmentOutput {
  return FragmentOutput(vec4(exposure));
}
)axsl";

  auto result = compiler.compile(source, "", "globals-set.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_EQ(result.merged_layout.resource_layout.value_blocks.size(), 1u);
  const auto &block = result.merged_layout.resource_layout.value_blocks[0];
  EXPECT_EQ(block.logical_name, "__globals");
  ASSERT_TRUE(block.descriptor_set.has_value());
  EXPECT_EQ(*block.descriptor_set, 3u);
}

TEST(LayoutMergeTest, RejectsConflictingValueBlockMetadata) {
  ShaderPipelineLayout merged;
  ShaderPipelineLayout incoming;
  LayoutMergeState state;
  std::vector<std::string> errors;

  merged.resource_layout.value_blocks.push_back(ShaderValueBlockDesc{
      .block_id = 1u,
      .logical_name = "camera",
      .descriptor_set = 0u,
      .binding = 0u,
      .size = 64u,
      .fields =
          {
              ShaderValueFieldDesc{
                  .binding_id = shader_binding_id("camera.projection"),
                  .logical_name = "camera.projection",
                  .type = TypeRef{TokenKind::TypeMat4, ""},
                  .stage_mask = shader_stage_mask(StageKind::Vertex),
                  .offset = 0u,
                  .size = 64u,
                  .array_stride = 0u,
                  .matrix_stride = 16u,
              },
          },
  });
  state.value_block_sources["camera"] = "vertex.axsl";
  state.value_field_sources["camera::camera.projection"] = "vertex.axsl";

  incoming.resource_layout.value_blocks.push_back(ShaderValueBlockDesc{
      .block_id = 1u,
      .logical_name = "camera",
      .descriptor_set = 1u,
      .binding = 0u,
      .size = 64u,
      .fields =
          {
              ShaderValueFieldDesc{
                  .binding_id = shader_binding_id("camera.projection"),
                  .logical_name = "camera.projection",
                  .type = TypeRef{TokenKind::TypeMat4, ""},
                  .stage_mask = shader_stage_mask(StageKind::Fragment),
                  .offset = 0u,
                  .size = 64u,
                  .array_stride = 0u,
                  .matrix_stride = 16u,
              },
          },
  });

  EXPECT_FALSE(merge_pipeline_layout_checked(
      merged, incoming, "fragment.axsl", state, errors
  ));
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(errors.front().find("camera"), std::string::npos);
  EXPECT_NE(errors.front().find("descriptor sets differ"), std::string::npos);
}

} // namespace
} // namespace astralix
