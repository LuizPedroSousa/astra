#include "output.hpp"

#include "shader-lang/compiler.hpp"
#include <filesystem>
#include <gtest/gtest.h>

TEST(AxslcOutput, WritesReflectionSidecarAlongsideStageFiles) {
  static constexpr std::string_view src = R"axsl(
@version 450;

interface VertexOutput {
    @location(0) vec4 color;
}

@vertex
fn main() -> VertexOutput {
    return VertexOutput(vec4(1.0));
}
  )axsl";

  astralix::Compiler compiler;
  auto result = compiler.compile(src, {}, "/tmp/axslc-output-test.axsl",
                                 {
                                     .emit_binding_cpp = true,
                                     .emit_reflection_ir = true,
                                     .reflection_ir_format = astralix::SerializationFormat::Json,
                                 });
  ASSERT_TRUE(result.ok());

  const auto output_dir = std::filesystem::temp_directory_path() /
                          "astralix-axslc-output-test";
  std::filesystem::create_directories(output_dir);

  std::vector<std::filesystem::path> written_paths;
  std::string error;
  ASSERT_TRUE(write_outputs(result, "light.axsl", output_dir, &error,
                            &written_paths))
      << error;

  EXPECT_TRUE(std::filesystem::exists(output_dir / "light.vert.glsl"));
  EXPECT_TRUE(std::filesystem::exists(output_dir / "light.reflection.json"));
  EXPECT_TRUE(std::filesystem::exists(output_dir / "light.reflection.hpp"));

  std::filesystem::remove_all(output_dir);
}
