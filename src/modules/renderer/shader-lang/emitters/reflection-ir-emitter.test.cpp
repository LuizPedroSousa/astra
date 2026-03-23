#include "shader-lang/emitters/reflection-ir-emitter.hpp"

#include "shader-lang/compiler.hpp"
#include <gtest/gtest.h>

namespace astralix {

namespace {

std::string errors_str(const CompileResult &result) {
  std::string message;
  for (const auto &error : result.errors) {
    message += error + "\n";
  }
  return message;
}

} // namespace

TEST(ReflectionIREmitter, EmitsJsonReflectionIRFromReflection) {
  static constexpr std::string_view src = R"axsl(
@version 450;

@uniform
interface Entity {
    mat4 projection;
}

interface VertexOutput {
    @location(0) vec4 color;
}

@vertex
fn main(Entity entity) -> VertexOutput {
    return VertexOutput(vec4(entity.projection[0][0]));
}
)axsl";

  Compiler compiler;
  auto result = compiler.compile(src, {}, "entity.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  ReflectionIREmitter emitter;
  std::string error;
  auto artifact =
      emitter.emit(result.reflection, SerializationFormat::Json, &error);
  ASSERT_TRUE(artifact.has_value()) << error;

  EXPECT_EQ(artifact->format, SerializationFormat::Json);
  EXPECT_EQ(artifact->extension, ".json");
  EXPECT_NE(artifact->content.find("\"version\""), std::string::npos);
  EXPECT_NE(artifact->content.find("\"entity.projection\""), std::string::npos);
}

TEST(ReflectionIREmitter, CompilerCanOptionallyEmitReflectionIR) {
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

  Compiler compiler;
  auto result =
      compiler.compile(src, {}, "light.axsl",
                       {
                           .emit_reflection_ir = true,
                           .reflection_ir_format = SerializationFormat::Json,
                       });
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_TRUE(result.reflection_ir.has_value());
  EXPECT_EQ(result.reflection_ir->format, SerializationFormat::Json);
  EXPECT_EQ(result.reflection_ir->extension, ".json");
  EXPECT_NE(result.reflection_ir->content.find("\"stage\""), std::string::npos);
}

} // namespace astralix
