#include "shader-lang/lowering/glsl-lowering.hpp"

#include "shader-lang/linker.hpp"
#include "shader-lang/lowering/canonical-lowering.hpp"
#include "shader-lang/parser.hpp"
#include "shader-lang/tokenizer.hpp"
#include <algorithm>
#include <gtest/gtest.h>

namespace astralix {

namespace {

struct LinkedShaderProgram {
  Program program;
  LinkResult link_result;
  std::vector<std::string> errors;

  bool ok() const { return errors.empty(); }
};

std::pair<std::vector<Token>, std::vector<std::string>>
tokenize_source(std::string_view src, std::string_view filename = "") {
  Tokenizer tokenizer(src, filename);
  std::vector<Token> tokens;

  while (true) {
    Token token = tokenizer.next();
    tokens.push_back(token);
    if (token.kind == TokenKind::EoF) {
      break;
    }
  }

  return {std::move(tokens), tokenizer.errors()};
}

LinkedShaderProgram link_source(std::string_view source) {
  LinkedShaderProgram result;

  auto [tokens, token_errors] = tokenize_source(source, "test.axsl");
  if (!token_errors.empty()) {
    result.errors = std::move(token_errors);
    return result;
  }

  Parser parser(std::move(tokens), source);
  result.program = parser.parse();
  if (!parser.errors().empty()) {
    result.errors = parser.errors();
    return result;
  }

  Linker linker;
  result.link_result = linker.link(result.program, parser.nodes(), {});
  if (!result.link_result.ok()) {
    result.errors = result.link_result.errors;
  }

  return result;
}

CanonicalLoweringResult lower_canonical_stage(std::string_view source,
                                              StageKind stage) {
  LinkedShaderProgram linked = link_source(source);
  EXPECT_TRUE(linked.ok());

  CanonicalLowering lowering(linked.link_result.all_nodes);
  auto result = lowering.lower(linked.program, linked.link_result, stage);
  EXPECT_TRUE(result.ok());
  return result;
}

GlslLoweringResult lower_glsl_result(std::string_view source, StageKind stage) {
  auto canonical_result = lower_canonical_stage(source, stage);
  GLSLLowering lowering;
  auto result =
      lowering.lower(canonical_result.stage, canonical_result.reflection);
  EXPECT_TRUE(result.ok());
  return result;
}

GLSLStage lower_glsl_stage(std::string_view source, StageKind stage) {
  return std::move(lower_glsl_result(source, stage).stage);
}

} // namespace

TEST(GLSLLoweringPass, VertexStageParamsLowerToFlatInputVariables) {
  static constexpr std::string_view src = R"axsl(
@version 450;

@in
interface VertexInput {
    @location(0) vec3 position;
    @location(1) vec2 texture_coordinate;
}

interface VertexOutput {
    @location(0) vec2 texture_coordinate;
}

@vertex
fn main(VertexInput input) -> VertexOutput {
    return VertexOutput(input.texture_coordinate);
}
)axsl";

  GLSLStage stage = lower_glsl_stage(src, StageKind::Vertex);

  std::vector<std::string> input_names;
  size_t input_block_count = 0;
  for (const auto &decl : stage.declarations) {
    if (const auto *global_var = std::get_if<GLSLGlobalVarDecl>(&decl)) {
      if (global_var->storage == "in") {
        input_names.push_back(global_var->name);
      }
    } else if (const auto *block = std::get_if<GLSLInterfaceBlockDecl>(&decl)) {
      if (block->storage == "in") {
        ++input_block_count;
      }
    }
  }

  ASSERT_EQ(input_names.size(), 2u);
  EXPECT_EQ(input_names[0], "position");
  EXPECT_EQ(input_names[1], "texture_coordinate");
  EXPECT_EQ(input_block_count, 0u);
}

TEST(GLSLLoweringPass, FragmentOutputsLowerToSplitOutVariables) {
  static constexpr std::string_view src = R"axsl(
@version 450;

interface FragmentOutput {
    @location(0) vec4 color;
    @location(1) float bloom;
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(1.0), 1.0);
}
)axsl";

  GLSLStage stage = lower_glsl_stage(src, StageKind::Fragment);

  std::vector<std::string> output_names;
  size_t output_block_count = 0;
  for (const auto &decl : stage.declarations) {
    if (const auto *global_var = std::get_if<GLSLGlobalVarDecl>(&decl)) {
      if (global_var->storage == "out") {
        output_names.push_back(global_var->name);
      }
    } else if (const auto *block = std::get_if<GLSLInterfaceBlockDecl>(&decl)) {
      if (block->storage == "out") {
        ++output_block_count;
      }
    }
  }

  ASSERT_EQ(output_names.size(), 2u);
  EXPECT_EQ(output_names[0], "_out_color");
  EXPECT_EQ(output_names[1], "_out_bloom");
  EXPECT_EQ(output_block_count, 0u);
}

TEST(GLSLLoweringPass, ResourceAliasCollisionsReceiveStableFallbackNames) {
  static constexpr std::string_view src = R"axsl(
@version 450;

@uniform
interface MaterialA {
    vec4 color;
}

@uniform
interface MaterialB {
    vec4 color;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(MaterialA a, MaterialB b) -> FragmentOutput {
    return FragmentOutput(a.color + b.color);
}
)axsl";

  GLSLStage stage = lower_glsl_stage(src, StageKind::Fragment);

  std::vector<std::string> uniform_names;
  for (const auto &decl : stage.declarations) {
    const auto *global_var = std::get_if<GLSLGlobalVarDecl>(&decl);
    if (!global_var || global_var->storage != "uniform") {
      continue;
    }

    uniform_names.push_back(global_var->name);
  }

  EXPECT_NE(std::find(uniform_names.begin(), uniform_names.end(), "_color"),
            uniform_names.end());
  EXPECT_NE(std::find(uniform_names.begin(), uniform_names.end(), "_b_color"),
            uniform_names.end());
}

TEST(GLSLLoweringPass, OnlyReachableHelpersProducePrototypesAndDefinitions) {
  static constexpr std::string_view src = R"axsl(
@version 450;

interface FragmentOutput {
    @location(0) vec4 color;
}

fn leaf(float x) -> float {
    return x * 2.;
}

fn helper(float x) -> float {
    return leaf(x);
}

fn unused_helper(float x) -> float {
    return x + 1.;
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(helper(1.)));
}
)axsl";

  GLSLStage stage = lower_glsl_stage(src, StageKind::Fragment);

  std::vector<std::string> prototypes;
  std::vector<std::string> definitions;
  for (const auto &decl : stage.declarations) {
    const auto *function_decl = std::get_if<GLSLFunctionDecl>(&decl);
    if (!function_decl || function_decl->name == "main") {
      continue;
    }

    if (function_decl->prototype_only) {
      prototypes.push_back(function_decl->name);
    } else {
      definitions.push_back(function_decl->name);
    }
  }

  ASSERT_EQ(prototypes.size(), 2u);
  ASSERT_EQ(definitions.size(), 2u);
  EXPECT_EQ(prototypes[0], "leaf");
  EXPECT_EQ(prototypes[1], "helper");
  EXPECT_EQ(definitions[0], "leaf");
  EXPECT_EQ(definitions[1], "helper");
}

TEST(GLSLLoweringPass, ReflectionTracksEmittedUniformAliases) {
  static constexpr std::string_view src = R"axsl(
@version 450;

@uniform
interface Entity {
    mat4 view;
}

interface VertexOutput {
    @location(0) vec4 color;
}

@vertex
fn main(Entity entity) -> VertexOutput {
    return VertexOutput(vec4(entity.view[0][0]));
}
)axsl";

  auto result = lower_glsl_result(src, StageKind::Vertex);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.reflection.resources.size(), 1u);

  const auto &resource = result.reflection.resources.front();
  ASSERT_EQ(resource.members.size(), 1u);
  EXPECT_EQ(resource.members.front().logical_name, "entity.view");
  ASSERT_TRUE(resource.members.front().glsl.emitted_name.has_value());
  EXPECT_EQ(*resource.members.front().glsl.emitted_name, "_view");
}

} // namespace astralix
