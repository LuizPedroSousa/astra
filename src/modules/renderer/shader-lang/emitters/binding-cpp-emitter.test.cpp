#include "shader-lang/emitters/binding-cpp-emitter.hpp"

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

TEST(BindingCppEmitter, EmitsTypedBindingHeaderFromReflection) {
  static constexpr std::string_view src = R"axsl(
@version 450;

struct Exposure {
    vec3 ambient;
    vec3 diffuse;
};

@uniform
interface Light {
    Exposure exposure;
    mat4 matrices[2];
    bool enabled = false;
}

interface VertexOutput {
    @location(0) vec4 color;
}

@vertex
fn main(Light light) -> VertexOutput {
    return VertexOutput(vec4(light.exposure.ambient + light.matrices[0][0].xyz, 1.0));
}
)axsl";

  Compiler compiler;
  auto result = compiler.compile(src, {}, "src/shaders/light.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  BindingCppEmitter emitter;
  std::string error;
  auto header =
      emitter.emit(result.reflection, "src/shaders/light.axsl", &error);
  ASSERT_TRUE(header.has_value()) << error;

  EXPECT_NE(header->find("namespace astralix::shader_bindings::src_shaders_light_axsl"),
            std::string::npos);
  EXPECT_NE(header->find("struct LightUniform"), std::string::npos);
  EXPECT_NE(header->find("struct LightParams"), std::string::npos);
  EXPECT_NE(header->find("struct exposure__ambient_t"), std::string::npos);
  EXPECT_NE(header->find("static constexpr std::string_view logical_name = \"light.exposure.ambient\""),
            std::string::npos);
  EXPECT_NE(header->find("std::array<glm::mat4, 2>"), std::string::npos);
  EXPECT_NE(header->find("enabled = false;"), std::string::npos);
  EXPECT_NE(header->find("apply_shader_params"), std::string::npos);
}

TEST(BindingCppEmitter, SuffixesDuplicateInterfaceContainersByInstanceName) {
  static constexpr std::string_view src = R"axsl(
@version 450;

@uniform
interface Light {
    vec3 color;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(Light key, Light fill) -> FragmentOutput {
    return FragmentOutput(vec4(key.color + fill.color, 1.0));
}
)axsl";

  Compiler compiler;
  auto result = compiler.compile(src, {}, "lighting.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  BindingCppEmitter emitter;
  std::string error;
  auto header = emitter.emit(result.reflection, "lighting.axsl", &error);
  ASSERT_TRUE(header.has_value()) << error;

  EXPECT_NE(header->find("struct LightKeyUniform"), std::string::npos);
  EXPECT_NE(header->find("struct LightFillUniform"), std::string::npos);
  EXPECT_NE(header->find("struct LightKeyParams"), std::string::npos);
  EXPECT_NE(header->find("struct LightFillParams"), std::string::npos);
}

TEST(BindingCppEmitter, MergesStageMasksAcrossStages) {
  static constexpr std::string_view src = R"axsl(
@version 450;

@uniform
interface Entity {
    mat4 view;
    float exposure = 1.0;
}

interface VertexOutput {
    @location(0) vec4 color;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@vertex
fn main(Entity entity) -> VertexOutput {
    return VertexOutput(vec4(entity.view[0][0]));
}

@fragment
fn main(Entity entity) -> FragmentOutput {
    return FragmentOutput(vec4(entity.view[0][0] * entity.exposure));
}
)axsl";

  Compiler compiler;
  auto result = compiler.compile(src, {}, "entity.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  BindingCppEmitter emitter;
  std::string error;
  auto header = emitter.emit(result.reflection, "entity.axsl", &error);
  ASSERT_TRUE(header.has_value()) << error;

  EXPECT_NE(header->find("logical_name = \"entity.view\""), std::string::npos);
  EXPECT_NE(header->find("stage_mask = 3u;"), std::string::npos);
}

TEST(BindingCppEmitter, RejectsConflictingSchemasForSameLogicalResource) {
  ShaderReflection reflection;

  ResourceReflection vertex_resource;
  vertex_resource.logical_name = "entity";
  vertex_resource.kind = ShaderResourceKind::UniformInterface;
  vertex_resource.stage = StageKind::Vertex;
  vertex_resource.declared_name = "Entity";
  vertex_resource.type = TypeRef{TokenKind::Identifier, "Entity"};
  vertex_resource.declared_fields.push_back(DeclaredFieldReflection{
      "projection",
      "entity.projection",
      TypeRef{TokenKind::TypeMat4, "mat4"},
      std::nullopt,
      std::nullopt,
      shader_stage_mask(StageKind::Vertex),
      shader_binding_id("entity.projection"),
      {}});
  reflection.stages[StageKind::Vertex].stage = StageKind::Vertex;
  reflection.stages[StageKind::Vertex].resources.push_back(vertex_resource);

  ResourceReflection fragment_resource = vertex_resource;
  fragment_resource.stage = StageKind::Fragment;
  fragment_resource.declared_fields[0].type =
      TypeRef{TokenKind::TypeVec4, "vec4"};
  reflection.stages[StageKind::Fragment].stage = StageKind::Fragment;
  reflection.stages[StageKind::Fragment].resources.push_back(fragment_resource);

  BindingCppEmitter emitter;
  std::string error;
  auto header = emitter.emit(reflection, "conflict.axsl", &error);
  EXPECT_FALSE(header.has_value());
  EXPECT_NE(error.find("conflicting typed uniform schema"), std::string::npos);
}

TEST(BindingCppEmitter, EmitsSamplerFieldsAsResourceBindings) {
  static constexpr std::string_view src = R"axsl(
@version 450;

interface VertexOutput {
    @location(0) vec3 texture_coordinate;
}

interface FragmentInput {
    @uniform samplerCube skybox;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(VertexOutput vertex, FragmentInput fragment) -> FragmentOutput {
    return FragmentOutput(texture(fragment.skybox, vertex.texture_coordinate));
}
)axsl";

  Compiler compiler;
  auto result = compiler.compile(src, {}, "skybox.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  BindingCppEmitter emitter;
  std::string error;
  auto header = emitter.emit(result.reflection, "skybox.axsl", &error);
  ASSERT_TRUE(header.has_value()) << error;

  EXPECT_NE(header->find("struct FragmentInputResources"), std::string::npos);
  EXPECT_NE(header->find("struct skybox_t"), std::string::npos);
  EXPECT_NE(header->find("static constexpr std::string_view logical_name = \"fragment.skybox\""),
            std::string::npos);
  EXPECT_EQ(header->find("int skybox = -1;"), std::string::npos);
  EXPECT_EQ(header->find("using value_type = int;"), std::string::npos);
}

TEST(BindingCppEmitter, CompilerCanOptionallyEmitBindingHeader) {
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
  auto result = compiler.compile(src, {}, "entity.axsl",
                                 {.emit_binding_cpp = true});
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_TRUE(result.binding_cpp_header.has_value());
  EXPECT_NE(result.binding_cpp_header->find("struct EntityUniform"),
            std::string::npos);
  EXPECT_NE(result.binding_cpp_header->find("namespace astralix::shader_bindings::entity_axsl"),
            std::string::npos);
}

} // namespace astralix
