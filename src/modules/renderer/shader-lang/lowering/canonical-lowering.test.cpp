#include "shader-lang/lowering/canonical-lowering.hpp"

#include "shader-lang/linker.hpp"
#include "shader-lang/parser.hpp"
#include "shader-lang/tokenizer.hpp"
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

CanonicalLoweringResult lower_stage(std::string_view source, StageKind stage) {
  LinkedShaderProgram linked = link_source(source);
  if (!linked.ok()) {
    CanonicalLoweringResult result;
    result.errors = std::move(linked.errors);
    return result;
  }

  CanonicalLowering lowering(linked.link_result.all_nodes);
  return lowering.lower(linked.program, linked.link_result, stage);
}

std::vector<const CanonicalStmt *>
top_level_stmts(const CanonicalStage &stage) {
  std::vector<const CanonicalStmt *> stmts;
  const auto *block = std::get_if<CanonicalBlockStmt>(&stage.entry.body->data);
  if (!block) {
    return stmts;
  }

  for (const auto &stmt : block->stmts) {
    stmts.push_back(stmt.get());
  }

  return stmts;
}

} // namespace

TEST(CanonicalLowering, ReachableHelperFunctionsArePrunedPerStage) {
  static constexpr std::string_view src = R"axsl(
@version 450;

interface FragmentOutput {
    @location(0) vec4 color;
}

fn fragment_leaf(float x) -> float {
    return x * 2.;
}

fn fragment_helper(float x) -> float {
    return fragment_leaf(x);
}

fn unused_helper(float x) -> float {
    return x + 1.;
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(fragment_helper(1.)));
}
)axsl";

  auto result = lower_stage(src, StageKind::Fragment);
  ASSERT_TRUE(result.ok());

  std::vector<std::string> helper_names;
  for (const auto &decl : result.stage.declarations) {
    if (const auto *function_decl = std::get_if<CanonicalFunctionDecl>(&decl)) {
      helper_names.push_back(function_decl->name);
    }
  }

  ASSERT_EQ(helper_names.size(), 2u);
  EXPECT_EQ(helper_names[0], "fragment_leaf");
  EXPECT_EQ(helper_names[1], "fragment_helper");
}

TEST(CanonicalLowering, ResourceInterfaceFieldsArePrunedBeforeBackendLowering) {
  static constexpr std::string_view src = R"axsl(
@version 450;

@uniform
interface Entity {
    mat4 view;
    mat4 projection;
    bool use_instancing = false;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(Entity entity) -> FragmentOutput {
    return FragmentOutput(vec4(entity.view[0][0]));
}
)axsl";

  auto result = lower_stage(src, StageKind::Fragment);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.stage.entry.resource_inputs.size(), 1u);

  const auto &binding = result.stage.entry.resource_inputs.front();
  ASSERT_EQ(binding.fields.size(), 1u);
  EXPECT_EQ(binding.fields.front().name, "view");
}

TEST(CanonicalLowering, ReflectionKeepsDeclaredUniformInterfaceTree) {
  static constexpr std::string_view src = R"axsl(
@version 450;

struct Exposure {
    vec3 ambient;
    vec3 diffuse;
};

@uniform
interface Light {
    Exposure exposure;
    float intensity = 2.0;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(Light light) -> FragmentOutput {
    return FragmentOutput(vec4(light.exposure.ambient, 1.0));
}
)axsl";

  auto result = lower_stage(src, StageKind::Fragment);
  ASSERT_TRUE(result.ok());

  ASSERT_EQ(result.reflection.resources.size(), 1u);
  const auto &resource = result.reflection.resources.front();

  ASSERT_EQ(resource.members.size(), 2u);
  EXPECT_EQ(resource.members[0].logical_name, "light.exposure.ambient");
  EXPECT_EQ(resource.members[1].logical_name, "light.exposure.diffuse");

  ASSERT_EQ(resource.declared_fields.size(), 2u);
  const auto &exposure = resource.declared_fields[0];
  EXPECT_EQ(exposure.logical_name, "light.exposure");
  ASSERT_EQ(exposure.fields.size(), 2u);
  EXPECT_EQ(exposure.fields[0].logical_name, "light.exposure.ambient");
  EXPECT_EQ(exposure.fields[0].active_stage_mask,
            shader_stage_mask(StageKind::Fragment));
  EXPECT_EQ(exposure.fields[1].logical_name, "light.exposure.diffuse");
  EXPECT_EQ(exposure.fields[1].active_stage_mask,
            shader_stage_mask(StageKind::Fragment));

  const auto &intensity = resource.declared_fields[1];
  EXPECT_EQ(intensity.logical_name, "light.intensity");
  ASSERT_TRUE(intensity.default_value.has_value());
  EXPECT_EQ(std::get<float>(*intensity.default_value), 2.0f);
  EXPECT_EQ(intensity.active_stage_mask, 0u);
}

TEST(CanonicalLowering, OutputAccumulatorIsNormalizedIntoOutputAssignments) {
  static constexpr std::string_view src = R"axsl(
@version 450;

interface VertexOutput {
    @location(0) vec3 position;
}

@vertex
fn main() -> VertexOutput {
    VertexOutput output;
    output.position = vec3(1.0);
    return output;
}
)axsl";

  auto result = lower_stage(src, StageKind::Vertex);
  ASSERT_TRUE(result.ok());

  auto stmts = top_level_stmts(result.stage);
  ASSERT_EQ(stmts.size(), 2u);
  EXPECT_TRUE(
      std::holds_alternative<CanonicalOutputAssignStmt>(stmts[0]->data));

  const auto &assign = std::get<CanonicalOutputAssignStmt>(stmts[0]->data);
  EXPECT_EQ(assign.field, "position");
  EXPECT_TRUE(std::holds_alternative<CanonicalReturnStmt>(stmts[1]->data));
}

TEST(CanonicalLowering, StructuredReturnIsNormalizedIntoAssignmentsAndReturn) {
  static constexpr std::string_view src = R"axsl(
@version 450;

interface FragmentOutput {
    @location(0) vec4 color;
    @location(1) float bloom = 1.0;
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(1.0));
}
)axsl";

  auto result = lower_stage(src, StageKind::Fragment);
  ASSERT_TRUE(result.ok());

  auto stmts = top_level_stmts(result.stage);
  ASSERT_EQ(stmts.size(), 2u);
  EXPECT_TRUE(
      std::holds_alternative<CanonicalOutputAssignStmt>(stmts[0]->data));
  ASSERT_TRUE(std::holds_alternative<CanonicalBlockStmt>(stmts[1]->data));

  const auto &default_assign =
      std::get<CanonicalOutputAssignStmt>(stmts[0]->data);
  const auto &structured_return_block =
      std::get<CanonicalBlockStmt>(stmts[1]->data);
  ASSERT_EQ(structured_return_block.stmts.size(), 2u);
  ASSERT_TRUE(std::holds_alternative<CanonicalOutputAssignStmt>(
      structured_return_block.stmts[0]->data));
  ASSERT_TRUE(std::holds_alternative<CanonicalReturnStmt>(
      structured_return_block.stmts[1]->data));

  const auto &return_assign = std::get<CanonicalOutputAssignStmt>(
      structured_return_block.stmts[0]->data);

  EXPECT_EQ(default_assign.field, "bloom");
  EXPECT_EQ(return_assign.field, "color");
}

TEST(CanonicalLowering, UniformInterfaceProducesLogicalReflectionMembers) {
  static constexpr std::string_view src = R"axsl(
@version 450;

@uniform
interface Entity {
    mat4 view;
    mat4 projection;
    bool use_instancing = false;
}

interface VertexOutput {
    @location(0) vec4 color;
}

@vertex
fn main(Entity entity) -> VertexOutput {
    return VertexOutput(vec4(entity.view[0][0] + entity.projection[0][0]));
}
)axsl";

  auto result = lower_stage(src, StageKind::Vertex);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.reflection.resources.size(), 1u);

  const auto &resource = result.reflection.resources.front();
  EXPECT_EQ(resource.logical_name, "entity");
  EXPECT_EQ(resource.kind, ShaderResourceKind::UniformInterface);
  ASSERT_EQ(resource.members.size(), 2u);
  EXPECT_EQ(resource.members[0].logical_name, "entity.view");
  ASSERT_TRUE(resource.members[0].compatibility_alias.has_value());
  EXPECT_EQ(*resource.members[0].compatibility_alias, "view");
  EXPECT_EQ(resource.members[1].logical_name, "entity.projection");
  ASSERT_TRUE(resource.members[1].compatibility_alias.has_value());
  EXPECT_EQ(*resource.members[1].compatibility_alias, "projection");
}

TEST(CanonicalLowering, ConflictingFlatAliasesAreSuppressedInReflection) {
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

  auto result = lower_stage(src, StageKind::Fragment);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.reflection.resources.size(), 2u);

  for (const auto &resource : result.reflection.resources) {
    ASSERT_EQ(resource.members.size(), 1u);
    EXPECT_FALSE(resource.members.front().compatibility_alias.has_value());
  }
}

} // namespace astralix
