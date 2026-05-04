#include "shader-lang/compiler.hpp"
#include "shader-lang/artifacts/shader-artifact-pipeline.hpp"
#include "shader-lang/diagnostics.hpp"
#include "shader-lang/emitters/glsl-text-emitter.hpp"
#include "shader-lang/lowering/glsl-stage-clone.hpp"
#include "shader-lang/lowering/vulkan-glsl-layout-pass.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <set>

namespace astralix {

static constexpr std::string_view k_full_source = R"axsl(
@version 450;

struct Material {
    vec3 albedo;
    float roughness;
};

const float PI = 3.14159;

@in
interface Attributes {
    @location(0) vec3 a_pos;
    @location(1) vec2 a_uv;
    @location(2) vec3 a_normal;
}

interface Varyings {
    vec2 v_uv;
    vec3 v_normal;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@binding(0) uniform mat4 u_mvp;
@binding(1) uniform mat4 u_model;
@binding(2) uniform MaterialBlock {
    Material u_mat;
} materials;
@binding(3) uniform sampler2D u_tex;

fn gamma(vec3 c) -> vec3 {
    return pow(c, vec3(1.0 / 2.2));
}

@vertex
fn main(Attributes a) -> Varyings {
    vec3 normal = vec3(u_model * vec4(a.a_normal, 0.0));
    gl_Position = u_mvp * vec4(a.a_pos, 1.0);
    return Varyings(a.a_uv, normal);
}

@fragment
fn main(Varyings v) -> FragmentOutput {
    vec4 sample = texture(u_tex, v.v_uv);
    if (sample.a < 0.1) {
        discard;
    }
    return FragmentOutput(
        vec4(gamma(sample.rgb * materials.u_mat.albedo), sample.a));
}
)axsl";

static bool contains(const std::string &src, std::string_view needle) {
  return src.find(needle) != std::string::npos;
}

static size_t count_occurrences(const std::string &src,
                                std::string_view needle) {
  size_t count = 0;
  size_t position = 0;

  while ((position = src.find(needle, position)) != std::string::npos) {
    ++count;
    position += needle.size();
  }

  return count;
}

static std::string errors_str(const CompileResult &r) {
  std::string out;
  for (const auto &e : r.errors) {
    out += e;
    out += '\n';
  }
  return out;
}

static std::string make_fragment_program(
    std::string_view body, std::string_view globals = {},
    std::string_view signature = "fn main() -> FragmentOutput",
    std::string_view output_interface = R"axsl(
interface FragmentOutput {
    @location(0) vec4 color;
}
)axsl") {
  std::string source = "@version 450;\n";

  if (!globals.empty()) {
    source += "\n";
    source += globals;
    if (source.back() != '\n') {
      source += '\n';
    }
  }

  if (!output_interface.empty()) {
    source += "\n";
    source += output_interface;
    if (source.back() != '\n') {
      source += '\n';
    }
  }

  source += "\n@fragment\n";
  source += signature;
  source += " {\n";
  source += body;
  if (!body.empty() && body.back() != '\n') {
    source += '\n';
  }
  source += "}\n";
  return source;
}

static GLSLExprPtr make_test_float_literal(double value) {
  auto expr = std::make_unique<GLSLExpr>();
  expr->type = TypeRef{TokenKind::TypeFloat, "float"};
  expr->data = GLSLLiteralExpr{value};
  return expr;
}

static bool same_plan(const ShaderArtifactPlan &lhs,
                      const ShaderArtifactPlan &rhs) {
  if (lhs.total_shaders != rhs.total_shaders ||
      lhs.generated_shaders != rhs.generated_shaders ||
      lhs.unchanged_shaders != rhs.unchanged_shaders ||
      lhs.failed_shaders != rhs.failed_shaders ||
      lhs.planned_removals != rhs.planned_removals ||
      lhs.watched_paths != rhs.watched_paths || lhs.deletes != rhs.deletes ||
      lhs.failures.size() != rhs.failures.size() ||
      lhs.writes.size() != rhs.writes.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.writes.size(); ++i) {
    if (lhs.writes[i].path != rhs.writes[i].path ||
        lhs.writes[i].content != rhs.writes[i].content) {
      return false;
    }
  }

  for (size_t i = 0; i < lhs.failures.size(); ++i) {
    if (lhs.failures[i].canonical_id != rhs.failures[i].canonical_id ||
        lhs.failures[i].source_path != rhs.failures[i].source_path ||
        lhs.failures[i].message != rhs.failures[i].message) {
      return false;
    }
  }

  return true;
}

TEST(ShaderCompiler, CompileSucceeds) {
  Compiler compiler;
  auto result = compiler.compile(k_full_source);
  EXPECT_TRUE(result.ok()) << errors_str(result);
  EXPECT_EQ(result.stages.size(), 2u);
  EXPECT_NE(result.stages.find(StageKind::Vertex), result.stages.end());
  EXPECT_NE(result.stages.find(StageKind::Fragment), result.stages.end());
}

TEST(ShaderCompiler, BuildArtifactPlanMatchesPipeline) {
  const auto root =
      std::filesystem::temp_directory_path() / "astralix-compiler-plan";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "shaders");

  const auto shader_path = root / "shaders" / "light.axsl";
  {
    std::ofstream shader(shader_path);
    shader << k_full_source;
  }

  const std::vector<ShaderArtifactInput> inputs = {{
      .canonical_id = "project/shaders/light.axsl",
      .source_path = shader_path,
      .output_root = root,
      .umbrella_name = "project_shaders.hpp",
  }};

  Compiler compiler;
  ShaderArtifactPipeline pipeline;

  auto via_compiler = compiler.build_artifact_plan(inputs);
  auto via_pipeline = pipeline.build_plan(inputs);

  EXPECT_TRUE(same_plan(via_compiler, via_pipeline));

  std::filesystem::remove_all(root);
}

TEST(ShaderCompiler, VersionHeader) {
  Compiler compiler;
  auto result = compiler.compile(k_full_source);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Vertex), "#version 450 core"));
  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Fragment), "#version 450 core"));
}

TEST(ShaderCompiler, ReachableGlobalStructEmittedPerStage) {
  Compiler compiler;
  auto result = compiler.compile(k_full_source);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  EXPECT_FALSE(
      contains(result.stages.at(StageKind::Vertex), "struct Material {"));
  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Fragment), "struct Material {"));
}

TEST(ShaderCompiler, StructFieldInitializerEmitted) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    Material material;
    return FragmentOutput(vec4(material.roughness));
)axsl",
                                   R"axsl(
struct Material {
    float roughness = 0.5;
};
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment),
                       "float roughness = 0.5;"));
}

TEST(ShaderCompiler, IncludedStructFieldInitializerEmitted) {
  Compiler compiler;

  const std::filesystem::path include_dir =
      std::filesystem::temp_directory_path() / "astralix-field-init-tests";
  std::filesystem::create_directories(include_dir);

  const std::filesystem::path include_path = include_dir / "material.axsl";
  {
    std::ofstream include_file(include_path);
    include_file << R"axsl(
struct Material {
    float roughness = 0.5;
};
)axsl";
  }

  static constexpr std::string_view src = R"axsl(
@version 450;
@include "material.axsl";

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main() -> FragmentOutput {
    Material material;
    return FragmentOutput(vec4(material.roughness));
}
)axsl";

  auto result = compiler.compile(src, include_dir.string(), "main.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment),
                       "float roughness = 0.5;"));
}

TEST(ShaderCompiler, GlobalConstEmitted) {
  Compiler compiler;
  auto result = compiler.compile(k_full_source);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(contains(result.stages.at(StageKind::Vertex), "const float PI"));
  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Fragment), "const float PI"));
}

TEST(ShaderCompiler, InterfaceBlockInlineEmitted) {
  Compiler compiler;
  auto result = compiler.compile(k_full_source);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Vertex), "layout(location = 0) in"));
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment),
                       "layout(location = 0) out vec4 _out_color;"));
}

TEST(ShaderCompiler, GlobalInterfaceResolvedInVertex) {
  Compiler compiler;
  auto result = compiler.compile(k_full_source);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &vert = result.stages.at(StageKind::Vertex);
  EXPECT_TRUE(contains(vert, "out Varyings {"));
  EXPECT_TRUE(contains(vert, "vec2 v_uv;"));
  EXPECT_TRUE(contains(vert, "vec3 v_normal;"));
}

TEST(ShaderCompiler, GlobalInterfaceResolvedInFragment) {
  Compiler compiler;
  auto result = compiler.compile(k_full_source);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "in Varyings {"));
  EXPECT_TRUE(contains(frag, "vec2 v_uv;"));
}

TEST(ShaderCompiler, UniformEmitted) {
  Compiler compiler;
  auto result = compiler.compile(k_full_source);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Vertex), "uniform mat4 u_mvp;"));
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment),
                       "uniform sampler2D u_tex;"));
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment),
                       "uniform MaterialBlock {"));
}

TEST(ShaderCompiler, BindingQualifierEmitted) {
  Compiler compiler;
  auto result = compiler.compile(k_full_source);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Vertex), "layout(binding = 0)"));
  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Fragment), "layout(binding = 3)"));
}

TEST(ShaderCompiler, BufferBlockBindingQualifierEmitted) {
  Compiler compiler;
  auto result = compiler.compile(k_full_source);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment),
                       "layout(binding = 2) uniform MaterialBlock {"));
}

TEST(ShaderCompiler, LocationQualifierOnField) {
  Compiler compiler;
  auto result = compiler.compile(k_full_source);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Vertex), "layout(location = 0)"));
  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Vertex), "layout(location = 1)"));
}

TEST(ShaderCompiler, StageIsolation) {
  Compiler compiler;
  auto result = compiler.compile(k_full_source);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_FALSE(contains(result.stages.at(StageKind::Vertex), "u_tex"));
  EXPECT_FALSE(contains(result.stages.at(StageKind::Vertex), "materials"));
  EXPECT_FALSE(contains(result.stages.at(StageKind::Fragment), "u_mvp"));
}

TEST(ShaderCompiler, LexError) {
  Compiler compiler;
  auto result = compiler.compile("@version 450;\n@unknown");
  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.errors.empty());
}

TEST(ShaderCompiler, ParseErrorUnclosedStage) {
  Compiler compiler;
  auto result = compiler.compile("@version 450;\n@vertex fn main() -> void {");
  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.errors.empty());
}

TEST(ShaderCompiler, EmptySource) {
  Compiler compiler;
  auto result = compiler.compile("@version 450;");
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.stages.empty());
}

TEST(ShaderCompiler, UniformArray) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    return FragmentOutput(vec4(point_lights[0].position, 1.0));
)axsl",
                                   R"axsl(
struct PointLight { vec3 position; };
uniform PointLight point_lights[4];
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment),
                       "uniform PointLight point_lights[4]"));
}

TEST(ShaderCompiler, WhileLoop) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    float x = 0.0;
    while (x < 1.0) { x += 0.1; }
    return FragmentOutput(vec4(x, x, x, 1.0));
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment), "while ("));
}

TEST(ShaderCompiler, LocalVarDecl) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    vec4 sample = vec4(1.0, 0.0, 0.0, 1.0);
    return FragmentOutput(sample);
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment), "vec4 sample"));
}

TEST(ShaderCompiler, LocalArrayDeclAndArrayConstructor) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    vec2 poisson_disk[2] = vec2[](vec2(0.0, 1.0), vec2(1.0, 0.0));
    vec2 sample = poisson_disk[1];
    return FragmentOutput(vec4(sample, 0.0, 1.0));
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(
      contains(
          frag,
          "vec2 poisson_disk[2] = vec2[](vec2(0.0, 1.0), vec2(1.0, 0.0));"
      ));
  EXPECT_TRUE(contains(frag, "vec2 sample = poisson_disk[1];"));
}

TEST(ShaderCompiler, HelperFunctionCanReturnFixedSizeArray) {
  Compiler compiler;
  auto src = make_fragment_program(
      R"axsl(
    float weights[2] = blur_kernel();
    return FragmentOutput(vec4(weights[1]));
)axsl",
      R"axsl(
fn blur_kernel() -> float[2] {
    float weights[2] = float[](0.25, 0.75);
    return weights;
}
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "float[2] blur_kernel()"));
  EXPECT_TRUE(contains(frag, "float weights[2] = blur_kernel();"));
}

TEST(ShaderCompiler, RuntimeSizedFunctionReturnIsRejected) {
  Compiler compiler;
  auto src = make_fragment_program(
      R"axsl(
    float weights[2] = blur_kernel();
    return FragmentOutput(vec4(weights[1]));
)axsl",
      R"axsl(
fn blur_kernel() -> float[] {
    float weights[2] = float[](0.25, 0.75);
    return weights;
}
)axsl");
  auto result = compiler.compile(src);
  ASSERT_FALSE(result.ok());

  const std::string err = errors_str(result);
  EXPECT_NE(err.find("runtime-sized array types are not allowed here"),
            std::string::npos);
}

TEST(ShaderCompiler, StageEntryArrayReturnIsRejected) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    float weights[2] = float[](0.25, 0.75);
    return weights;
)axsl",
                                   {},
                                   "fn main() -> float[2]", "");
  auto result = compiler.compile(src);
  ASSERT_FALSE(result.ok());

  const std::string err = errors_str(result);
  EXPECT_NE(err.find("stage entry function 'main' cannot return arrays"),
            std::string::npos);
}

TEST(ShaderCompiler, ForLoopInit) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    float r = 0.0;
    for (int i = 0; i < 4; ++i) { r += 0.25; }
    return FragmentOutput(vec4(r, r, r, 1.0));
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment), "int i = 0"));
}

TEST(ShaderCompiler, ConstLocalVar) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    const float scale = 2.0;
    return FragmentOutput(vec4(scale));
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Fragment), "const float scale"));
}

static size_t count_substr(const std::string &src, std::string_view needle) {
  size_t count = 0, pos = 0;
  while ((pos = src.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

TEST(ShaderCompiler, TrailingDotFloat) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    float a = 0.;
    float b = 1.;
    float c = 2.;
    return FragmentOutput(vec4(a + b + c));
)axsl");

  auto result = compiler.compile(src);

  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &fragment = result.stages.at(StageKind::Fragment);

  EXPECT_TRUE(contains(fragment, "float a"));
  EXPECT_TRUE(contains(fragment, "float b"));
  EXPECT_TRUE(contains(fragment, "float c"));
}

TEST(ShaderCompiler, TernaryExpression) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    float a = 1.0;
    float x = (a > 0.) ? 1. : 0.;
    return FragmentOutput(vec4(x));
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment), "? 1.0"));
}

TEST(ShaderCompiler, UniformBlock) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    mat4 model = instance.models[0];
    return FragmentOutput(vec4(model[0][0]));
)axsl",
                                   R"axsl(
@binding(0) uniform InstanceBuffer { mat4 models[]; } instance;
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "layout(binding = 0) uniform InstanceBuffer {"));
  EXPECT_TRUE(contains(frag, "mat4 models[];"));
  EXPECT_TRUE(contains(frag, "} instance;"));
}

TEST(ShaderCompiler, InlineInterfaceStorageBlockEmitted) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    mat4 model = instance.models[0];
    return FragmentOutput(vec4(model[0][0]));
)axsl",
                                   R"axsl(
@std430
@binding(0)
in InstanceBuffer {
    mat4 models[];
} instance;
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(
      contains(frag, "layout(std430, binding = 0) buffer InstanceBuffer {"));
  EXPECT_TRUE(contains(frag, "mat4 models[];"));
  EXPECT_TRUE(contains(frag, "} instance;"));
}

TEST(ShaderCompiler, UniformWithDefault) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
      return FragmentOutput(flag ? vec4(near_plane) : vec4(0.0));
)axsl",
                                   R"axsl(
uniform float near_plane = -10.0;
uniform bool flag = false;
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "uniform float near_plane = "));
  EXPECT_TRUE(contains(frag, "uniform bool flag = false"));
}

TEST(ShaderCompiler, NestedForLoops) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
      float sum = 0.0;
      for (int x = -1; x <= 1; ++x) {
          for (int y = -1; y <= 1; ++y) {
              float v = 0.;
              sum += v;
          }
      }
      return FragmentOutput(vec4(sum));
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_GE(count_substr(result.stages.at(StageKind::Fragment), "for ("), 2u);
}

TEST(ShaderCompiler, BreakContinue) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
  float r = 0.0;
  for (int i = 0; i < 4; ++i) {
      if (i == 2) { break; }
      continue;
  }
  return FragmentOutput(vec4(r));
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "break;"));
  EXPECT_TRUE(contains(frag, "continue;"));
}

TEST(ShaderCompiler, DiscardStatement) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
  float alpha = 0.5;

  if (alpha < 0.1) {
      discard;
  }

  return FragmentOutput(vec4(1.0));
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment), "discard;"));
}

TEST(ShaderCompiler, ArrayIndexing) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    vec3 v = lights[0].position;
    return FragmentOutput(vec4(v, 1.0));
)axsl",
                                   R"axsl(
struct PointLight { vec3 position; };
uniform PointLight lights[4];
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment), "lights[0]"));
}

TEST(ShaderCompiler, CompoundAssignment) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    float x = 0.;
    x += 1.;
    x -= 0.5;
    x *= 2.;
    return FragmentOutput(vec4(x));
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "+="));
  EXPECT_TRUE(contains(frag, "-="));
  EXPECT_TRUE(contains(frag, "*="));
}

TEST(ShaderCompiler, OutInterfaceBlockWithLocations) {
  Compiler compiler;
  auto src = make_fragment_program(
      R"axsl(
    return FragOut(vec4(1.0), vec4(0.0));
)axsl",
      {}, "fn main() -> FragOut", R"axsl(
interface FragOut {
    @location(0) vec4 color;
    @location(1) vec4 bright;
}
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "layout(location = 0) out vec4 _out_color"));
  EXPECT_TRUE(contains(frag, "layout(location = 1) out vec4 _out_bright"));
}

TEST(ShaderCompiler, UndefinedVariableError) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;
interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main() -> FragmentOutput {
    vec3 pos = undefined_var.position;
    return FragmentOutput(vec4(pos, 1.0));
}
)axsl";
  auto result = compiler.compile(src);
  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.errors.empty());
  const auto err = errors_str(result);
  EXPECT_TRUE(err.find("undefined identifier") != std::string::npos);
  EXPECT_TRUE(err.find("undefined_var") != std::string::npos)
      << "Expected identifier name in error: " << err;
}

TEST(ShaderCompiler, LocalVariableReference) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    Material material;
    material.albedo = vec3(1.0, 0.5, 0.2);
    return FragmentOutput(vec4(material.albedo, 1.0));
)axsl",
                                   R"axsl(
struct Material { vec3 albedo; float roughness; };
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Fragment), "material.albedo"));
}

TEST(ShaderCompiler, StageFunctionParameterReference) {
  Compiler compiler;
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
fn main(VertexOutput vertex, FragmentInput fragment) -> void {
    vec4 color = texture(fragment.skybox, vertex.texture_coordinate);
    return FragmentOutput(color);
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "in VertexOutput {"));
  EXPECT_TRUE(contains(frag, "uniform samplerCube _skybox;"));
  EXPECT_TRUE(contains(frag, "layout(location = 0) out vec4 _out_color;"));
  EXPECT_TRUE(contains(frag, "texture(_skybox, vertex.texture_coordinate)"));
  EXPECT_TRUE(contains(frag, "_out_color = color;"));
  EXPECT_TRUE(contains(frag, "vertex.texture_coordinate"));
}

TEST(ShaderCompiler, VertexStageFunctionParameterReference) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

uniform mat4 view_without_transformation;
uniform mat4 projection;

@in
interface VertexInput {
    @location(0) vec3 position;
}

interface VertexOutput {
    @location(0) vec3 texture_coordinate;
}

@vertex
fn main(VertexInput input) -> VertexOutput {
    vec4 pos = projection * view_without_transformation * vec4(input.position, 1.);
    gl_Position = pos.xyww;
    return VertexOutput(input.position);
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  const auto &vert = result.stages.at(StageKind::Vertex);
  EXPECT_TRUE(contains(vert, "layout(location = 0) in vec3 position;"));
  EXPECT_FALSE(contains(vert, "in VertexInput {"));
  EXPECT_TRUE(contains(vert, "_stage_out.texture_coordinate = position;"));
}

TEST(ShaderCompiler, InterfaceFieldInitializerAppliedToVertexOutput) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

interface VertexOutput {
    @location(0) vec3 texture_coordinate = vec3(0.0);
}

@vertex
fn main() -> VertexOutput {
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    return VertexOutput();
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &vert = result.stages.at(StageKind::Vertex);
  EXPECT_TRUE(contains(vert, "out VertexOutput {"));
  EXPECT_TRUE(contains(vert, "layout(location = 0) vec3 texture_coordinate;"));
  EXPECT_FALSE(
      contains(vert, "layout(location = 0) vec3 texture_coordinate = vec3"));
  EXPECT_TRUE(contains(vert, "_stage_out.texture_coordinate = vec3(0.0);"));
  EXPECT_FALSE(contains(vert, "return VertexOutput();"));
  EXPECT_TRUE(contains(vert, "return;"));
}

TEST(ShaderCompiler, StructuredReturnDoesNotDuplicateOutputDefaults) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

uniform samplerCube skybox;

@in
interface VertexInput {
    @location(0) vec3 position;
    @location(2) vec3 texture_coordinate;
}

interface FragmentOutput {
    @location(0) vec4 color;
    @location(1) float test = 0.0;
    @location(2) bool has_value = true;
}

@fragment
fn main(VertexInput input) -> FragmentOutput {
    vec4 color = texture(skybox, input.texture_coordinate);
    return FragmentOutput(color);
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_EQ(count_occurrences(frag, "_out_test = 0.0;"), 1u);
  EXPECT_EQ(count_occurrences(frag, "_out_has_value = true;"), 1u);
  EXPECT_EQ(count_occurrences(frag, "_out_color = color;"), 1u);
}

TEST(ShaderCompiler, VertexOutputAccumulatorLocalLowersToStageOutput) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

@in
interface VertexInput {
    @location(0) vec3 position;
}

interface VertexOutput {
    @location(0) vec3 world_position;
}

@vertex
fn main(VertexInput input) -> VertexOutput {
    VertexOutput output;
    output.world_position = input.position;
    gl_Position = vec4(input.position, 1.0);
    return output;
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &vert = result.stages.at(StageKind::Vertex);
  EXPECT_TRUE(contains(vert, "out VertexOutput {"));
  EXPECT_TRUE(contains(vert, "} output;"));
  EXPECT_FALSE(contains(vert, "VertexOutput output;"));
  EXPECT_TRUE(contains(vert, "output.world_position = position;"));
  EXPECT_FALSE(contains(vert, "return output;"));
  EXPECT_TRUE(contains(vert, "return;"));
}

TEST(ShaderCompiler, FragmentOutputAccumulatorLocalLowersToSplitOutputs) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

interface VertexOutput {
    @location(0) vec2 texture_coordinate;
}

interface FragmentOutput {
    @location(0) vec4 color;
    @location(1) float brightness = 0.0;
}

@fragment
fn main(VertexOutput input) -> FragmentOutput {
    FragmentOutput output;
    output.color = vec4(input.texture_coordinate, 0.0, 1.0);
    output.brightness = output.color.r;
    return output;
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "layout(location = 0) out vec4 _out_color;"));
  EXPECT_TRUE(
      contains(frag, "layout(location = 1) out float _out_brightness;"));
  EXPECT_FALSE(contains(frag, "FragmentOutput output;"));
  EXPECT_TRUE(
      contains(frag, "_out_color = vec4(input.texture_coordinate, 0.0, 1.0);"));
  EXPECT_TRUE(contains(frag, "_out_brightness = _out_color.r;"));
  EXPECT_FALSE(contains(frag, "return output;"));
  EXPECT_TRUE(contains(frag, "return;"));
}

TEST(ShaderCompiler, OutputAccumulatorRejectsMultipleLocals) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

interface VertexOutput {
    @location(0) vec3 world_position;
}

@vertex
fn main() -> VertexOutput {
    VertexOutput output;
    VertexOutput other;
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    return output;
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_FALSE(result.ok());
  const auto err = errors_str(result);
  EXPECT_NE(err.find("supports only one local of type 'VertexOutput'"),
            std::string::npos)
      << err;
}

TEST(ShaderCompiler, OutputAccumulatorRejectsInitializer) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

interface VertexOutput {
    @location(0) vec3 world_position;
}

@vertex
fn main() -> VertexOutput {
    VertexOutput output = VertexOutput(vec3(1.0));
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    return output;
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_FALSE(result.ok());
  const auto err = errors_str(result);
  EXPECT_NE(
      err.find(
          "stage entry output accumulator 'output' cannot have an initializer"),
      std::string::npos)
      << err;
}

TEST(ShaderCompiler, OutputAccumulatorRejectsWholeValueAssignment) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

interface VertexOutput {
    @location(0) vec3 world_position;
}

@vertex
fn main() -> VertexOutput {
    VertexOutput output;
    output = VertexOutput(vec3(1.0));
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    return output;
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_FALSE(result.ok());
  const auto err = errors_str(result);
  EXPECT_NE(err.find("stage entry output accumulator 'output' cannot be "
                     "assigned as a whole value"),
            std::string::npos)
      << err;
}

TEST(ShaderCompiler, OutputAccumulatorRejectsReturningDifferentLocal) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

interface VertexOutput {
    @location(0) vec3 world_position;
}

@vertex
fn main() -> VertexOutput {
    VertexOutput output;
    VertexOutput other;
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    return other;
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_FALSE(result.ok());
  const auto err = errors_str(result);
  EXPECT_NE(
      err.find(
          "stage entry output accumulator must return 'output', not 'other'"),
      std::string::npos)
      << err;
}

TEST(ShaderCompiler, OutputAccumulatorRejectsNonFieldValueUse) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

interface VertexOutput {
    @location(0) vec3 world_position;
}

@vertex
fn main() -> VertexOutput {
    VertexOutput output;
    int value = output;
    gl_Position = vec4(float(value), 0.0, 0.0, 1.0);
    return output;
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_FALSE(result.ok());
  const auto err = errors_str(result);
  EXPECT_NE(err.find("stage entry output accumulator 'output' can only be used "
                     "via field access or 'return output'"),
            std::string::npos)
      << err;
}

TEST(ShaderCompiler, ForwardDeclaresGlobalFunctions) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    return FragmentOutput(vec4(call_first(1.0)));
)axsl",
                                   R"axsl(
fn call_first(float x) -> float {
    return later(x);
}

fn later(float x) -> float {
    return x * 2.0;
}
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &frag = result.stages.at(StageKind::Fragment);
  auto later_proto = frag.find("float later(float x);");
  auto later_def = frag.find("float later(float x) {");
  ASSERT_NE(later_proto, std::string::npos) << frag;
  ASSERT_NE(later_def, std::string::npos) << frag;
  EXPECT_LT(later_proto, later_def);
}

TEST(ShaderCompiler, IncludedFunctionsAreForwardDeclared) {
  Compiler compiler;

  const std::filesystem::path include_dir =
      std::filesystem::temp_directory_path() / "astralix-forward-decl-tests";
  std::filesystem::create_directories(include_dir);

  const std::filesystem::path include_path = include_dir / "later.axsl";
  {
    std::ofstream include_file(include_path);
    include_file << R"axsl(
fn later(float x) -> float {
    return x * 2.0;
}
)axsl";
  }

  static constexpr std::string_view src = R"axsl(
@version 450;
@include "later.axsl";

interface FragmentOutput {
    @location(0) vec4 color;
}

fn call_first(float x) -> float {
    return later(x);
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(call_first(1.0)));
}
)axsl";

  auto result = compiler.compile(src, include_dir.string(), "main.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &frag = result.stages.at(StageKind::Fragment);
  auto later_proto = frag.find("float later(float x);");
  auto later_def = frag.find("float later(float x) {");
  ASSERT_NE(later_proto, std::string::npos) << frag;
  ASSERT_NE(later_def, std::string::npos) << frag;
  EXPECT_LT(later_proto, later_def);
}

TEST(ShaderCompiler, ResolvesNestedIncludesRelativeToIncludingFile) {
  Compiler compiler;

  const std::filesystem::path include_dir =
      std::filesystem::temp_directory_path() / "astralix-nested-include-tests";
  const std::filesystem::path helpers_dir = include_dir / "helpers";
  std::filesystem::create_directories(helpers_dir);

  {
    std::ofstream include_file(include_dir / "material.axsl");
    include_file << R"axsl(
@include "helpers/constants.axsl";

struct Material {
    float roughness = MATERIAL_ROUGHNESS;
};
)axsl";
  }

  {
    std::ofstream include_file(helpers_dir / "constants.axsl");
    include_file << R"axsl(
const float MATERIAL_ROUGHNESS = 0.5;
)axsl";
  }

  static constexpr std::string_view src = R"axsl(
@version 450;
@include "material.axsl";

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main() -> FragmentOutput {
    Material material;
    return FragmentOutput(vec4(material.roughness));
}
)axsl";

  auto result = compiler.compile(src, include_dir.string(), "main.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment),
                       "const float MATERIAL_ROUGHNESS = 0.5;"));
  EXPECT_EQ(result.dependencies.size(), 2u);
  EXPECT_EQ(result.dependencies[0], include_dir / "material.axsl");
  EXPECT_EQ(result.dependencies[1], helpers_dir / "constants.axsl");

  std::filesystem::remove_all(include_dir);
}

TEST(ShaderCompiler, StageOnlyEmitsReachableGlobalFunctions) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

interface FragmentOutput {
    @location(0) vec4 color;
}

fn fragment_helper(float x) -> float {
    return fragment_leaf(x);
}

fn fragment_leaf(float x) -> float {
    return x * 2.0;
}

@vertex
fn main() -> void {
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(fragment_helper(1.0)));
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &vert = result.stages.at(StageKind::Vertex);
  EXPECT_FALSE(contains(vert, "float fragment_helper(float x);"));
  EXPECT_FALSE(contains(vert, "float fragment_helper(float x) {"));
  EXPECT_FALSE(contains(vert, "float fragment_leaf(float x);"));
  EXPECT_FALSE(contains(vert, "float fragment_leaf(float x) {"));

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "float fragment_helper(float x);"));
  EXPECT_TRUE(contains(frag, "float fragment_helper(float x) {"));
  EXPECT_TRUE(contains(frag, "float fragment_leaf(float x);"));
  EXPECT_TRUE(contains(frag, "float fragment_leaf(float x) {"));
}

TEST(ShaderCompiler, StageOnlyEmitsReachableStructs) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

struct LightExposure {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct DirectionalLight {
    LightExposure exposure;
    vec3 direction;
};

struct PointLight {
    vec3 position;
    LightExposure exposure;
};

uniform DirectionalLight directional_light;
uniform PointLight point_light;

interface FragmentOutput {
    @location(0) vec4 color;
}

@vertex
fn main() -> void {
    gl_Position = vec4(directional_light.exposure.ambient, 1.0);
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(point_light.exposure.diffuse, 1.0));
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &vert = result.stages.at(StageKind::Vertex);
  EXPECT_TRUE(contains(vert, "struct LightExposure {"));
  EXPECT_TRUE(contains(vert, "struct DirectionalLight {"));
  EXPECT_FALSE(contains(vert, "struct PointLight {"));

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "struct LightExposure {"));
  EXPECT_TRUE(contains(frag, "struct PointLight {"));
}

TEST(ShaderCompiler, BuiltInVariableReference) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;
interface FragmentOutput {
    @location(0) vec4 color;
}

@vertex
fn main() -> void {
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}

@fragment
fn main() -> FragmentOutput {
    vec4 coord = gl_FragCoord;
    return FragmentOutput(coord);
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(contains(result.stages.at(StageKind::Vertex), "gl_Position"));
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment), "gl_FragCoord"));
}

TEST(ShaderCompiler, UniformArrayReference) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    vec3 light_pos = lights[0].position;
    return FragmentOutput(vec4(light_pos, 1.0));
)axsl",
                                   R"axsl(
struct PointLight { vec3 position; vec3 color; };
uniform PointLight lights[4];
)axsl");
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment), "lights[0]"));
}

TEST(ShaderCompiler, InterfaceBlockInstanceValidation) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

interface Coords {
    vec2 uv;
    vec4 light_space_pos;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(Coords obj_coordinates) -> FragmentOutput {
    vec4 pos = obj_coordinates.light_space_pos;
    return FragmentOutput(pos);
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  EXPECT_TRUE(
      contains(result.stages.at(StageKind::Fragment), "obj_coordinates"));
}

TEST(ShaderCompiler, GlobalInlineInterfaceInstanceReference) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

in Test {
    @location(1) vec3 a;
} test;

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(test.a, 1.0));
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "in Test {"));
  EXPECT_TRUE(contains(frag, "} test;"));
  EXPECT_TRUE(contains(frag, "test.a"));
}

TEST(ShaderCompiler, MixedSymbolSourcesResolve) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

uniform float exposure;

interface VertexOutput {
    @location(0) vec3 texture_coordinate;
}

interface FragmentResources {
    @uniform samplerCube skybox;
}

in Test {
    @location(1) vec3 tint;
} test;

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(VertexOutput vertex, FragmentResources fragment) -> FragmentOutput {
    vec4 sample = texture(fragment.skybox, vertex.texture_coordinate);
    return FragmentOutput(vec4(sample.rgb * exposure + test.tint, sample.a));
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "uniform float exposure"));
  EXPECT_TRUE(contains(frag, "uniform samplerCube _skybox"));
  EXPECT_TRUE(contains(frag, "in Test {"));
  EXPECT_TRUE(contains(frag, "} test;"));
  EXPECT_TRUE(contains(frag, "texture(_skybox, vertex.texture_coordinate)"));
  EXPECT_TRUE(contains(frag, "test.tint"));
}

TEST(ShaderCompiler, UniformInterfaceRoleOnStageParamCullsUnusedFields) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

@uniform
interface Entity {
    mat4 view;
    mat4 projection;
    mat4 g_model;
    mat4 light_space_matrix;
    bool use_instacing = false;
    float unused_scale = 2.0;
}

@vertex
fn main(Entity entity) -> void {
    gl_Position = entity.projection * entity.view * vec4(0.0, 0.0, 0.0, 1.0);
    if (entity.use_instacing) {
        gl_Position = entity.g_model * gl_Position;
    }
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &vert = result.stages.at(StageKind::Vertex);
  EXPECT_FALSE(contains(vert, "in Entity {"));
  EXPECT_TRUE(contains(vert, "uniform mat4 _view;"));
  EXPECT_TRUE(contains(vert, "uniform mat4 _projection;"));
  EXPECT_TRUE(contains(vert, "uniform mat4 _g_model;"));
  EXPECT_TRUE(contains(vert, "uniform bool _use_instacing = false;"));
  EXPECT_FALSE(contains(vert, "_light_space_matrix"));
  EXPECT_FALSE(contains(vert, "_unused_scale"));
  EXPECT_TRUE(contains(
      vert, "gl_Position = (_projection * _view) * vec4(0.0, 0.0, 0.0, 1.0);"));
  EXPECT_TRUE(contains(vert, "if (_use_instacing)"));
  EXPECT_TRUE(contains(vert, "gl_Position = _g_model * gl_Position;"));
}

TEST(ShaderCompiler, FieldAnnotatedResourceParamCullsUnusedFields) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

interface VertexOutput {
    @location(0) vec3 texture_coordinate;
}

interface FragmentResources {
    @uniform samplerCube skybox;
    @uniform float exposure = 1.0;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(VertexOutput vertex, FragmentResources fragment) -> FragmentOutput {
    vec4 sample = texture(fragment.skybox, vertex.texture_coordinate);
    return FragmentOutput(sample);
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "uniform samplerCube _skybox;"));
  EXPECT_FALSE(contains(frag, "_exposure"));
  EXPECT_TRUE(contains(frag, "texture(_skybox, vertex.texture_coordinate)"));
}

TEST(ShaderCompiler, NestedResourceFieldUseKeepsBaseFieldEmitted) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

struct Material {
    sampler2D diffuse;
    sampler2D specular;
};

interface VertexOutput {
    @location(0) vec2 texture_coordinate;
}

interface FragmentResources {
    @uniform Material materials[1];
    @uniform float unused_strength = 1.0;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(VertexOutput vertex, FragmentResources fragment) -> FragmentOutput {
    vec4 sample = texture(fragment.materials[0].diffuse, vertex.texture_coordinate);
    return FragmentOutput(sample);
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "uniform Material _materials[1];"));
  EXPECT_FALSE(contains(frag, "_unused_strength"));
  EXPECT_TRUE(contains(
      frag, "texture(_materials[0].diffuse, vertex.texture_coordinate)"));
}

TEST(ShaderCompiler, StructuredReturnInsideNestedControlFlow) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

interface VertexOutput {
    @location(0) vec3 texture_coordinate;
}

interface FragmentResources {
    @uniform samplerCube skybox;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(VertexOutput vertex, FragmentResources fragment) -> FragmentOutput {
    vec4 sample = texture(fragment.skybox, vertex.texture_coordinate);
    float factor = 0.25;

    if (sample.r > 0.5) {
        for (int i = 0; i < 2; ++i) {
            factor += sample.g > 0.25 ? 0.5 : 0.25;
        }
    }

    return FragmentOutput(vec4(sample.rgb * factor, sample.a));
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "texture(_skybox, vertex.texture_coordinate)"));
  EXPECT_TRUE(contains(frag, "if (sample.r > 0.5)"));
  EXPECT_TRUE(contains(frag, "for (int i = 0; i < 2; ++i)"));
  EXPECT_TRUE(contains(frag, "sample.g > 0.25 ? 0.5 : 0.25"));
  EXPECT_TRUE(
      contains(frag, "_out_color = vec4(sample.rgb * factor, sample.a);"));
}

TEST(ShaderCompiler, GlobalUniformSharedAcrossStages) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

uniform mat4 view_projection;

interface FragmentOutput {
    @location(0) vec4 color;
}

@vertex
fn main() -> void {
    gl_Position = view_projection * vec4(0.0, 0.0, 0.0, 1.0);
}

@fragment
fn main() -> FragmentOutput {
    mat4 vp = view_projection;
    return FragmentOutput(vec4(vp[0][0], 0.0, 0.0, 1.0));
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &vert = result.stages.at(StageKind::Vertex);
  const auto &frag = result.stages.at(StageKind::Fragment);

  EXPECT_TRUE(contains(vert, "uniform mat4 view_projection"));
  EXPECT_TRUE(contains(frag, "uniform mat4 view_projection"));
}

TEST(ShaderCompiler, GlobalUniformUsageFiltering) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

uniform mat4 model_matrix;

interface FragmentOutput {
    @location(0) vec4 color;
}

@vertex
fn main() -> void {
    gl_Position = model_matrix * vec4(0.0, 0.0, 0.0, 1.0);
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(1.0, 0.0, 0.0, 1.0));
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &vert = result.stages.at(StageKind::Vertex);
  EXPECT_TRUE(contains(vert, "uniform mat4 model_matrix"));

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_FALSE(contains(frag, "model_matrix"));
}

TEST(ShaderCompiler, UniformShadowing) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

uniform float global_value = 1.0;

interface FragmentResources {
    @uniform float global_value;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@vertex
fn main() -> void {
    float x = global_value;
    gl_Position = vec4(x, 0.0, 0.0, 1.0);
}

@fragment
fn main(FragmentResources resources) -> FragmentOutput {
    float x = resources.global_value;
    return FragmentOutput(vec4(x, 0.0, 0.0, 1.0));
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  EXPECT_TRUE(result.errors.empty());

  const auto &vert = result.stages.at(StageKind::Vertex);
  EXPECT_TRUE(contains(vert, "uniform float global_value = 1.0"));

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_FALSE(contains(frag, "uniform float global_value = 1.0"));
  EXPECT_TRUE(contains(frag, "uniform float _global_value"));
}

TEST(ShaderCompiler, GlobalUniformBufferSharing) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

@binding(0) uniform CameraData {
    mat4 view;
    mat4 projection;
} camera;

interface FragmentOutput {
    @location(0) vec4 color;
}

@vertex
fn main() -> void {
    gl_Position = camera.projection * camera.view * vec4(0.0, 0.0, 0.0, 1.0);
}

@fragment
fn main() -> FragmentOutput {
    mat4 view = camera.view;
    return FragmentOutput(vec4(view[0][0], 0.0, 0.0, 1.0));
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &vert = result.stages.at(StageKind::Vertex);
  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(vert, "uniform CameraData {"));
  EXPECT_TRUE(contains(vert, "} camera;"));
  EXPECT_TRUE(contains(frag, "uniform CameraData {"));
  EXPECT_TRUE(contains(frag, "} camera;"));
}

TEST(ShaderCompiler, ShadowingUniformBuffer) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

uniform LightData {
    vec3 position;
} light;

interface FragmentLight {
    @uniform vec3 position;
    @uniform vec3 color;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@vertex
fn main() -> void {
    vec3 pos = light.position;
    gl_Position = vec4(pos, 1.0);
}

@fragment
fn main(FragmentLight light) -> FragmentOutput {
    return FragmentOutput(vec4(light.color, 1.0));
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  EXPECT_TRUE(result.errors.empty());

  const auto &vert = result.stages.at(StageKind::Vertex);
  EXPECT_TRUE(contains(vert, "uniform LightData {"));
  EXPECT_TRUE(contains(vert, "vec3 position;"));
  EXPECT_FALSE(contains(vert, "vec3 color;"));

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_FALSE(contains(frag, "uniform LightData {"));
  EXPECT_FALSE(contains(frag, "uniform vec3 _position;"));
  EXPECT_TRUE(contains(frag, "uniform vec3 _color;"));
}

TEST(ShaderCompiler, UnusedGlobalUniformNotEmitted) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

uniform float unused_global;

@vertex
fn main() -> void {
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &vert = result.stages.at(StageKind::Vertex);
  EXPECT_FALSE(contains(vert, "unused_global"));
}

TEST(ShaderCompiler, GlobalUniformWithStageSpecific) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

uniform vec3 global_light_pos;
uniform mat4 model;

interface FragmentResources {
    @uniform vec3 frag_light_color;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@vertex
fn main() -> void {
    vec3 light = global_light_pos;
    gl_Position = model * vec4(light, 1.0);
}

@fragment
fn main(FragmentResources resources) -> FragmentOutput {
    vec3 light = global_light_pos;
    return FragmentOutput(vec4(resources.frag_light_color * light, 1.0));
}
)axsl";
  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &vert = result.stages.at(StageKind::Vertex);
  const auto &frag = result.stages.at(StageKind::Fragment);

  EXPECT_TRUE(contains(vert, "uniform vec3 global_light_pos"));
  EXPECT_TRUE(contains(frag, "uniform vec3 global_light_pos"));

  EXPECT_TRUE(contains(vert, "uniform mat4 model"));
  EXPECT_FALSE(contains(frag, "uniform mat4 model"));

  EXPECT_FALSE(contains(vert, "_frag_light_color"));
  EXPECT_TRUE(contains(frag, "uniform vec3 _frag_light_color"));
}

TEST(ShaderCompiler, ReflectionCarriesLogicalAndEmittedNamesForUniformInterfaces) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

@uniform
interface Entity {
    mat4 view;
    mat4 projection;
}

interface VertexOutput {
    @location(0) vec4 color;
}

@vertex
fn main(Entity entity) -> VertexOutput {
    return VertexOutput(vec4(entity.view[0][0] + entity.projection[0][0]));
}
)axsl";

  auto result = compiler.compile(src, "", "reflection.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  auto stage_it = result.reflection.stages.find(StageKind::Vertex);
  ASSERT_NE(stage_it, result.reflection.stages.end());
  ASSERT_EQ(stage_it->second.resources.size(), 1u);

  const auto &resource = stage_it->second.resources.front();
  ASSERT_EQ(resource.members.size(), 2u);
  EXPECT_EQ(resource.members[0].logical_name, "entity.view");
  ASSERT_TRUE(resource.members[0].compatibility_alias.has_value());
  EXPECT_EQ(*resource.members[0].compatibility_alias, "view");
  ASSERT_TRUE(resource.members[0].glsl.emitted_name.has_value());
  EXPECT_EQ(*resource.members[0].glsl.emitted_name, "_view");
  EXPECT_EQ(resource.members[1].logical_name, "entity.projection");
  ASSERT_TRUE(resource.members[1].glsl.emitted_name.has_value());
  EXPECT_EQ(*resource.members[1].glsl.emitted_name, "_projection");
}

TEST(ShaderCompiler, ReflectionKeepsDeclaredTreeForInactiveFieldsAndArrays) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

struct Bones {
    mat4 matrices[2];
};

@uniform
interface Entity {
    Bones bones;
    float intensity = 3.0;
}

interface VertexOutput {
    @location(0) vec4 color;
}

@vertex
fn main(Entity entity) -> VertexOutput {
    return VertexOutput(vec4(entity.bones.matrices[0][0][0]));
}
)axsl";

  auto result = compiler.compile(src, "", "reflection-arrays.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  auto stage_it = result.reflection.stages.find(StageKind::Vertex);
  ASSERT_NE(stage_it, result.reflection.stages.end());
  ASSERT_EQ(stage_it->second.resources.size(), 1u);

  const auto &resource = stage_it->second.resources.front();
  ASSERT_EQ(resource.members.size(), 2u);
  EXPECT_EQ(resource.members[0].logical_name, "entity.bones.matrices[0]");
  EXPECT_EQ(resource.members[0].binding_id,
            shader_binding_id("entity.bones.matrices[0]"));
  EXPECT_EQ(resource.members[1].logical_name, "entity.bones.matrices[1]");
  EXPECT_EQ(resource.members[1].binding_id,
            shader_binding_id("entity.bones.matrices[1]"));

  ASSERT_EQ(resource.declared_fields.size(), 2u);
  const auto &bones = resource.declared_fields[0];
  ASSERT_EQ(bones.fields.size(), 1u);
  const auto &matrices = bones.fields.front();
  EXPECT_EQ(matrices.logical_name, "entity.bones.matrices");
  EXPECT_EQ(matrices.array_size, 2u);
  EXPECT_EQ(matrices.binding_id, shader_binding_id("entity.bones.matrices"));
  EXPECT_EQ(matrices.active_stage_mask, shader_stage_mask(StageKind::Vertex));

  const auto &intensity = resource.declared_fields[1];
  ASSERT_TRUE(intensity.default_value.has_value());
  EXPECT_EQ(std::get<float>(*intensity.default_value), 3.0f);
  EXPECT_EQ(intensity.active_stage_mask, 0u);
}

TEST(ShaderCompiler, ReflectionSuppressesAmbiguousFlatAliases) {
  Compiler compiler;
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

  auto result = compiler.compile(src, "", "reflection-collision.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  auto stage_it = result.reflection.stages.find(StageKind::Fragment);
  ASSERT_NE(stage_it, result.reflection.stages.end());
  ASSERT_EQ(stage_it->second.resources.size(), 2u);

  for (const auto &resource : stage_it->second.resources) {
    ASSERT_EQ(resource.members.size(), 1u);
    EXPECT_FALSE(resource.members.front().compatibility_alias.has_value());
  }
}

TEST(ShaderCompiler, MergedLayoutPrefixesLooseGlobalBindingIds) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

uniform float bloom_strength = 0.12;
uniform float gamma = 2.2;
uniform float exposure = 1.0;

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(bloom_strength + gamma + exposure));
}
)axsl";

  auto result = compiler.compile(src, "", "globals-layout.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  ASSERT_EQ(result.merged_layout.resource_layout.value_blocks.size(), 1u);
  const auto &globals_block =
      result.merged_layout.resource_layout.value_blocks.front();
  EXPECT_EQ(globals_block.logical_name, "__globals");
  EXPECT_EQ(globals_block.size, 16u);
  ASSERT_EQ(globals_block.fields.size(), 3u);

  const ShaderValueFieldDesc *bloom_strength_field = nullptr;
  const ShaderValueFieldDesc *gamma_field = nullptr;
  const ShaderValueFieldDesc *exposure_field = nullptr;

  for (const auto &field : globals_block.fields) {
    if (field.logical_name == "bloom_strength") {
      bloom_strength_field = &field;
    } else if (field.logical_name == "gamma") {
      gamma_field = &field;
    } else if (field.logical_name == "exposure") {
      exposure_field = &field;
    }
  }

  ASSERT_NE(bloom_strength_field, nullptr);
  ASSERT_NE(gamma_field, nullptr);
  ASSERT_NE(exposure_field, nullptr);
  EXPECT_EQ(
      bloom_strength_field->binding_id,
      shader_binding_id("__globals.bloom_strength")
  );
  EXPECT_EQ(gamma_field->binding_id, shader_binding_id("__globals.gamma"));
  EXPECT_EQ(
      exposure_field->binding_id,
      shader_binding_id("__globals.exposure")
  );
  EXPECT_EQ(bloom_strength_field->offset, 0u);
  EXPECT_EQ(gamma_field->offset, 4u);
  EXPECT_EQ(exposure_field->offset, 8u);
}

TEST(ShaderCompiler, ReflectionPrefixesLooseGlobalBindingIds) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

uniform float bloom_strength = 0.12;
uniform float gamma = 2.2;
uniform float exposure = 1.0;

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(bloom_strength + gamma + exposure));
}
)axsl";

  auto result = compiler.compile(src, "", "globals-reflection.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  auto stage_it = result.reflection.stages.find(StageKind::Fragment);
  ASSERT_NE(stage_it, result.reflection.stages.end());
  ASSERT_EQ(stage_it->second.resources.size(), 3u);

  for (const auto &resource : stage_it->second.resources) {
    EXPECT_EQ(
        resource.binding_id,
        shader_binding_id("__globals." + resource.logical_name)
    );

    ASSERT_EQ(resource.members.size(), 1u);
    EXPECT_EQ(
        resource.members.front().binding_id,
        shader_binding_id("__globals." + resource.members.front().logical_name)
    );
  }
}

TEST(ShaderCompiler, MergedLayoutExpandsLooseGlobalArraysIntoIndexedFields) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

uniform vec3 samples[2];

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(samples[0] + samples[1], 1.0));
}
)axsl";

  auto result = compiler.compile(src, "", "globals-array-layout.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  ASSERT_EQ(result.merged_layout.resource_layout.value_blocks.size(), 1u);
  const auto &globals_block =
      result.merged_layout.resource_layout.value_blocks.front();
  EXPECT_EQ(globals_block.logical_name, "__globals");
  EXPECT_EQ(globals_block.size, 32u);
  ASSERT_EQ(globals_block.fields.size(), 2u);

  const ShaderValueFieldDesc *sample0 = nullptr;
  const ShaderValueFieldDesc *sample1 = nullptr;
  for (const auto &field : globals_block.fields) {
    if (field.logical_name == "samples[0]") {
      sample0 = &field;
    } else if (field.logical_name == "samples[1]") {
      sample1 = &field;
    }
  }

  ASSERT_NE(sample0, nullptr);
  ASSERT_NE(sample1, nullptr);
  EXPECT_EQ(sample0->binding_id, shader_binding_id("__globals.samples[0]"));
  EXPECT_EQ(sample1->binding_id, shader_binding_id("__globals.samples[1]"));
  EXPECT_EQ(sample0->offset, 0u);
  EXPECT_EQ(sample1->offset, 16u);
  EXPECT_EQ(sample0->size, 12u);
  EXPECT_EQ(sample1->size, 12u);
}

TEST(ShaderCompiler, MergedLayoutRoundsStd140BlockSizeToVec4Alignment) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

@uniform
interface Camera {
    vec3 position;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(Camera camera) -> FragmentOutput {
    return FragmentOutput(vec4(camera.position, 1.0));
}
)axsl";

  auto result = compiler.compile(src, "", "std140-camera-layout.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  ASSERT_EQ(result.merged_layout.resource_layout.value_blocks.size(), 1u);
  const auto &camera_block =
      result.merged_layout.resource_layout.value_blocks.front();
  EXPECT_EQ(camera_block.logical_name, "camera");
  EXPECT_EQ(camera_block.size, 16u);
  ASSERT_EQ(camera_block.fields.size(), 1u);
  EXPECT_EQ(camera_block.fields.front().offset, 0u);
  EXPECT_EQ(camera_block.fields.front().size, 12u);
}

TEST(ShaderCompiler, MergedLayoutExpandsUniformStructArraysIntoIndexedFields) {
  Compiler compiler;
  static constexpr std::string_view src = R"axsl(
@version 450;

struct Exposure {
    vec3 ambient;
    vec3 diffuse;
};

struct SampleLight {
    vec3 position;
    Exposure exposure;
    float intensity;
};

@uniform
interface Scene {
    SampleLight lights[2];
    float tail = 0.0;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(Scene scene) -> FragmentOutput {
    return FragmentOutput(vec4(scene.lights[1].exposure.diffuse + vec3(scene.tail), 1.0));
}
)axsl";

  auto result = compiler.compile(src, "", "uniform-struct-array-layout.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  ASSERT_EQ(result.merged_layout.resource_layout.value_blocks.size(), 1u);
  const auto &scene_block =
      result.merged_layout.resource_layout.value_blocks.front();
  EXPECT_EQ(scene_block.logical_name, "scene");
  EXPECT_EQ(scene_block.size, 144u);
  ASSERT_EQ(scene_block.fields.size(), 9u);

  const ShaderValueFieldDesc *lights0_position = nullptr;
  const ShaderValueFieldDesc *lights1_ambient = nullptr;
  const ShaderValueFieldDesc *lights1_diffuse = nullptr;
  const ShaderValueFieldDesc *lights1_intensity = nullptr;
  const ShaderValueFieldDesc *tail = nullptr;
  for (const auto &field : scene_block.fields) {
    if (field.logical_name == "scene.lights[0].position") {
      lights0_position = &field;
    } else if (field.logical_name == "scene.lights[1].exposure.ambient") {
      lights1_ambient = &field;
    } else if (field.logical_name == "scene.lights[1].exposure.diffuse") {
      lights1_diffuse = &field;
    } else if (field.logical_name == "scene.lights[1].intensity") {
      lights1_intensity = &field;
    } else if (field.logical_name == "scene.tail") {
      tail = &field;
    }
  }

  ASSERT_NE(lights0_position, nullptr);
  ASSERT_NE(lights1_ambient, nullptr);
  ASSERT_NE(lights1_diffuse, nullptr);
  ASSERT_NE(lights1_intensity, nullptr);
  ASSERT_NE(tail, nullptr);
  EXPECT_EQ(
      lights1_diffuse->binding_id,
      shader_binding_id("scene.lights[1].exposure.diffuse")
  );
  EXPECT_EQ(lights0_position->offset, 0u);
  EXPECT_EQ(lights1_ambient->offset, 80u);
  EXPECT_EQ(lights1_diffuse->offset, 96u);
  EXPECT_EQ(lights1_intensity->offset, 112u);
  EXPECT_EQ(tail->offset, 128u);
}

static std::string first_error(const CompileResult &r) {
  return r.errors.empty() ? "" : r.errors[0];
}

static bool validate_spirv_external(const std::vector<uint32_t> &spirv,
                                    const char *label,
                                    std::string *output = nullptr) {
  const std::string path =
      std::string("/tmp/astra_spirv_test_") + label + ".spv";
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(spirv.data()),
              static_cast<std::streamsize>(spirv.size() * sizeof(uint32_t)));
  }

  const std::string command =
      "spirv-val " + path + " --relax-block-layout --target-env vulkan1.3 2>&1";
  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) {
    if (output != nullptr) {
      *output = "failed to launch spirv-val";
    }
    return false;
  }

  char buffer[4096] = {};
  const size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, pipe);
  buffer[bytes_read] = 0;
  const int status = pclose(pipe);

  if (output != nullptr) {
    *output = buffer;
  }

  return status == 0;
}

static constexpr std::string_view k_unclosed_stage =
    "@version 450;\n"
    "@vertex fn main() -> void {";

TEST(ShaderDiagnostics, ErrorHasFilenameWhenProvided) {
  Compiler compiler;
  auto result = compiler.compile(k_unclosed_stage, "", "myshader.axsl");
  ASSERT_FALSE(result.ok());
  const auto err = first_error(result);
  EXPECT_NE(err.find("myshader.axsl:"), std::string::npos)
      << "Expected filename prefix in error:\n"
      << err;
}

TEST(ShaderDiagnostics, ErrorHasFileLineColFormat) {
  Compiler compiler;
  auto result = compiler.compile(k_unclosed_stage, "", "s.axsl");
  ASSERT_FALSE(result.ok());
  const auto err = first_error(result);
  auto file_pos = err.find("s.axsl:");
  ASSERT_NE(file_pos, std::string::npos) << err;
  auto after_file = file_pos + 7;
  EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(err[after_file])))
      << "Expected line digit after filename:\n"
      << err;
  auto colon2 = err.find(':', after_file);
  ASSERT_NE(colon2, std::string::npos) << err;
  EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(err[colon2 + 1])))
      << "Expected col digit after second colon:\n"
      << err;
  auto colon3 = err.find(':', colon2 + 1);
  ASSERT_NE(colon3, std::string::npos) << err;
  EXPECT_EQ(err[colon3 + 1], ' ') << "Expected space after file:line:col:\n"
                                  << err;
}

TEST(ShaderDiagnostics, ErrorHasNoFilePrefixWhenOmitted) {
  Compiler compiler;
  auto result = compiler.compile(k_unclosed_stage);
  ASSERT_FALSE(result.ok());
  const auto err = first_error(result);
  EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(err[0])))
      << "Expected line digit at start of error:\n"
      << err;
}

TEST(ShaderDiagnostics, ErrorContainsSourceContextSeparator) {
  Compiler compiler;
  auto result = compiler.compile(k_unclosed_stage, "", "ctx.axsl");
  ASSERT_FALSE(result.ok());
  const auto err = errors_str(result);
  EXPECT_NE(err.find(" | "), std::string::npos)
      << "Expected source context (\" | \") in error output:\n"
      << err;
}

TEST(ShaderDiagnostics, ErrorContainsCaretMarker) {
  Compiler compiler;
  auto result = compiler.compile(k_unclosed_stage, "", "caret.axsl");
  ASSERT_FALSE(result.ok());
  const auto err = errors_str(result);
  EXPECT_NE(err.find('^'), std::string::npos)
      << "Expected caret marker in error output:\n"
      << err;
}

TEST(ShaderDiagnostics, FormatSourceContextBasic) {
  std::string_view src = "aaa\nbbb\nccc\n";
  std::string ctx = format_source_context(src, 2, 2, 1);
  EXPECT_NE(ctx.find("1 | aaa"), std::string::npos) << ctx;
  EXPECT_NE(ctx.find("2 | bbb"), std::string::npos) << ctx;
  EXPECT_NE(ctx.find("3 | ccc"), std::string::npos) << ctx;
  EXPECT_NE(ctx.find(" | "), std::string::npos) << ctx;
  EXPECT_NE(ctx.find('^'), std::string::npos) << ctx;
}

TEST(ShaderDiagnostics, FormatSourceContextCaretColumn) {
  std::string_view src = "hello\nworld\n";
  std::string ctx = format_source_context(src, 1, 1, 0);
  EXPECT_NE(ctx.find("1 | hello"), std::string::npos) << ctx;
  EXPECT_NE(ctx.find("  | ^"), std::string::npos) << ctx;
}

TEST(ShaderDiagnostics, FormatSourceContextOutOfRange) {
  std::string_view src = "foo\n";
  EXPECT_EQ(format_source_context(src, 0, 1), "");
  EXPECT_EQ(format_source_context(src, 99, 1), "");
}

TEST(ShaderDiagnostics, ErrorMessageContainsErrorText) {
  Compiler compiler;
  auto result = compiler.compile(k_unclosed_stage, "", "msg.axsl");
  ASSERT_FALSE(result.ok());
  const auto err = errors_str(result);
  EXPECT_NE(err.find("expected"), std::string::npos)
      << "Expected 'expected' keyword in error message:\n"
      << err;
}

TEST(ShaderDiagnostics, GlobalScopeUnexpectedTokenHasContext) {
  static constexpr std::string_view src = "@version 450;\n"
                                          "if (true) {}\n";
  Compiler compiler;
  auto result = compiler.compile(src, "", "global.axsl");
  ASSERT_FALSE(result.ok());
  const auto err = first_error(result);
  EXPECT_NE(err.find("global.axsl:2:"), std::string::npos)
      << "Expected global-scope error on line 2:\n"
      << err;
  EXPECT_NE(err.find("unexpected token in global scope"), std::string::npos)
      << err;
  EXPECT_NE(err.find("expected a global declaration"), std::string::npos)
      << err;
  EXPECT_NE(err.find("2 | if (true) {}"), std::string::npos) << err;
}

TEST(ShaderDiagnostics, MissingSemicolonAfterOutBlockPointsToSameLine) {
  static constexpr std::string_view src = "@version 450;\n"
                                          "@binding(0) uniform CameraData {\n"
                                          "    mat4 view;\n"
                                          "} camera\n"
                                          "@vertex fn main() -> void {\n"
                                          "    gl_Position = vec4(0.0);\n"
                                          "}\n";
  Compiler compiler;
  auto result = compiler.compile(src, "", "t.axsl");
  ASSERT_FALSE(result.ok());
  const auto err = first_error(result);
  EXPECT_NE(err.find("t.axsl:4:"), std::string::npos)
      << "Expected error on line 4:\n"
      << err;
  EXPECT_EQ(err.find("t.axsl:5:"), std::string::npos)
      << "Error must not point to line 5 (next token):\n"
      << err;
}

TEST(ShaderDiagnostics, MissingSemicolonAfterUniformPointsToSameLine) {
  static constexpr std::string_view src = "@version 450;\n"
                                          "uniform float x\n"
                                          "@vertex fn main() -> void {\n"
                                          "    gl_Position = vec4(0.0);\n"
                                          "}\n";
  Compiler compiler;
  auto result = compiler.compile(src, "", "t.axsl");
  ASSERT_FALSE(result.ok());
  const auto err = first_error(result);
  EXPECT_NE(err.find("t.axsl:2:"), std::string::npos)
      << "Expected error on line 2:\n"
      << err;
  EXPECT_EQ(err.find("t.axsl:3:"), std::string::npos)
      << "Error must not point to line 3 (next token):\n"
      << err;
}

TEST(ShaderDiagnostics, MissingFieldNamePointsToSameLine) {
  static constexpr std::string_view src = "@version 450;\n"
                                          "struct Foo {\n"
                                          "    vec3 ;\n"
                                          "    float y;\n"
                                          "};\n";
  Compiler compiler;
  auto result = compiler.compile(src, "", "t.axsl");
  ASSERT_FALSE(result.ok());
  const auto err = first_error(result);
  EXPECT_NE(err.find("t.axsl:3:"), std::string::npos)
      << "Expected error on line 3:\n"
      << err;
  EXPECT_EQ(err.find("t.axsl:4:"), std::string::npos)
      << "Error must not point to line 4 (next token):\n"
      << err;
}

TEST(SPIRVEmitter, FullSourceProducesValidSPIRV) {
  Compiler compiler;
  CompileOptions options;
  options.emit_spirv = true;

  auto result = compiler.compile(k_full_source, "", "test.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  ASSERT_TRUE(result.spirv_stages.count(StageKind::Vertex))
      << "Missing vertex SPIR-V stage";
  ASSERT_TRUE(result.spirv_stages.count(StageKind::Fragment))
      << "Missing fragment SPIR-V stage";

  const auto &vertex_spirv = result.spirv_stages.at(StageKind::Vertex);
  const auto &fragment_spirv = result.spirv_stages.at(StageKind::Fragment);

  EXPECT_GT(vertex_spirv.size(), 5u) << "Vertex SPIR-V too small";
  EXPECT_GT(fragment_spirv.size(), 5u) << "Fragment SPIR-V too small";

  EXPECT_EQ(vertex_spirv[0], 0x07230203u) << "Bad SPIR-V magic number (vertex)";
  EXPECT_EQ(fragment_spirv[0], 0x07230203u)
      << "Bad SPIR-V magic number (fragment)";
}

TEST(SPIRVEmitter, MinimalVertexPassthrough) {
  static constexpr std::string_view source = R"axsl(
@version 450;

@in
interface Attributes {
    @location(0) vec3 position;
}

interface Varyings {
    vec3 v_pos;
}

@vertex
fn main(Attributes a) -> Varyings {
    gl_Position = vec4(a.position, 1.0);
    return Varyings(a.position);
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_spirv = true;

  auto result = compiler.compile(source, "", "passthrough.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_TRUE(result.spirv_stages.count(StageKind::Vertex));

  const auto &spirv = result.spirv_stages.at(StageKind::Vertex);
  EXPECT_EQ(spirv[0], 0x07230203u);
}

TEST(SPIRVEmitter, MinimalFragmentSolidColor) {
  static constexpr std::string_view source = R"axsl(
@version 450;

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(1.0, 0.0, 0.0, 1.0));
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_spirv = true;

  auto result = compiler.compile(source, "", "solid.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_TRUE(result.spirv_stages.count(StageKind::Fragment));

  const auto &spirv = result.spirv_stages.at(StageKind::Fragment);
  EXPECT_EQ(spirv[0], 0x07230203u);
}

TEST(SPIRVEmitter, SPIRVPassesExternalValidation) {
  Compiler compiler;
  CompileOptions options;
  options.emit_spirv = true;

  auto result = compiler.compile(k_full_source, "", "test.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  std::string output;
  EXPECT_TRUE(
      validate_spirv_external(result.spirv_stages.at(StageKind::Vertex),
                              "vertex", &output))
      << "spirv-val failed for vertex:\n"
      << output;
  EXPECT_TRUE(
      validate_spirv_external(result.spirv_stages.at(StageKind::Fragment),
                              "fragment", &output))
      << "spirv-val failed for fragment:\n"
      << output;
}

TEST(SPIRVEmitter, BuiltinInputStagesPassExternalValidation) {
  static constexpr std::string_view source = R"axsl(
@version 450;

@in
interface Attributes {
    @location(0) vec3 position;
}

interface Varyings {
    vec2 uv;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@vertex
fn main(Attributes a) -> Varyings {
    float offset = float(gl_InstanceID) * 0.1;
    gl_Position = vec4(a.position.x + offset, a.position.y, a.position.z, 1.0);
    return Varyings(a.position.xy);
}

@fragment
fn main(Varyings v) -> FragmentOutput {
    return FragmentOutput(vec4(gl_FragCoord.xy, v.uv.x, 1.0));
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_spirv = true;

  auto result = compiler.compile(source, "", "builtin-inputs.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  std::string output;
  EXPECT_TRUE(
      validate_spirv_external(result.spirv_stages.at(StageKind::Vertex),
                              "builtin-inputs-vertex", &output))
      << "spirv-val failed for builtin-inputs vertex:\n"
      << output;
  EXPECT_TRUE(
      validate_spirv_external(result.spirv_stages.at(StageKind::Fragment),
                              "builtin-inputs-fragment", &output))
      << "spirv-val failed for builtin-inputs fragment:\n"
      << output;
}

TEST(SPIRVEmitter, GLSLStagesStillEmittedAlongsideSPIRV) {
  Compiler compiler;
  CompileOptions options;
  options.emit_spirv = true;

  auto result = compiler.compile(k_full_source, "", "test.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  EXPECT_TRUE(result.stages.count(StageKind::Vertex))
      << "GLSL vertex stage missing when emit_spirv is on";
  EXPECT_TRUE(result.stages.count(StageKind::Fragment))
      << "GLSL fragment stage missing when emit_spirv is on";
  EXPECT_TRUE(result.spirv_stages.count(StageKind::Vertex));
  EXPECT_TRUE(result.spirv_stages.count(StageKind::Fragment));
}

TEST(Compiler, VulkanGLSLVertexStageAppliesDepthRangeFixup) {
  static constexpr std::string_view source = R"axsl(
@version 450;

@in
interface Attributes {
    @location(0) vec3 position;
}

@vertex
fn main(Attributes a) -> void {
    gl_Position = vec4(a.position, 1.0);
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_vulkan_glsl = true;

  auto result = compiler.compile(source, "", "vulkan-depth-remap.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_TRUE(result.vulkan_glsl_stages.count(StageKind::Vertex));

  const auto &vertex = result.vulkan_glsl_stages.at(StageKind::Vertex);
  EXPECT_FALSE(contains(vertex, "gl_Position.y = -gl_Position.y;"))
      << vertex;
  EXPECT_TRUE(
      contains(
          vertex, "gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;"
      )
  ) << vertex;
}

TEST(Compiler, CloneGLSLStagePreservesEmissionAndIsolatesMutations) {
  GLSLStage stage;
  stage.version = 450;

  GLSLFunctionDecl function;
  function.ret = TypeRef{TokenKind::KeywordVoid, "void"};
  function.name = "main";

  auto var_stmt = std::make_unique<GLSLStmt>();
  GLSLVarDeclStmt var_decl;
  var_decl.type = TypeRef{TokenKind::TypeFloat, "float"};
  var_decl.name = "value";
  var_decl.init = make_test_float_literal(1.0);
  var_stmt->data = std::move(var_decl);

  auto body = std::make_unique<GLSLStmt>();
  GLSLBlockStmt block;
  block.stmts.push_back(std::move(var_stmt));
  body->data = std::move(block);
  function.body = std::move(body);

  stage.declarations.push_back(GLSLDecl{std::move(function)});

  GLSLStage cloned = clone_glsl_stage(stage);

  GLSLTextEmitter emitter;
  const std::string original_text = emitter.emit(stage);
  EXPECT_EQ(original_text, emitter.emit(cloned));

  auto &clone_function = std::get<GLSLFunctionDecl>(cloned.declarations[0]);
  auto &clone_block = std::get<GLSLBlockStmt>(clone_function.body->data);
  auto *clone_var_decl = std::get_if<GLSLVarDeclStmt>(&clone_block.stmts[0]->data);
  ASSERT_NE(clone_var_decl, nullptr);
  auto *clone_literal = std::get_if<GLSLLiteralExpr>(&clone_var_decl->init->data);
  ASSERT_NE(clone_literal, nullptr);
  clone_literal->value = 2.0;

  EXPECT_EQ(original_text, emitter.emit(stage));
  EXPECT_NE(original_text, emitter.emit(cloned));
}

TEST(Compiler, GLSLTextEmitterKeepsFloatTypedLiteralsAsFloats) {
  GLSLStage stage;
  stage.version = 450;

  GLSLFunctionDecl function;
  function.ret = TypeRef{TokenKind::KeywordVoid, "void"};
  function.name = "main";

  auto float_stmt = std::make_unique<GLSLStmt>();
  GLSLVarDeclStmt float_decl;
  float_decl.type = TypeRef{TokenKind::TypeFloat, "float"};
  float_decl.name = "a";
  float_decl.init = std::make_unique<GLSLExpr>();
  float_decl.init->type = TypeRef{TokenKind::TypeFloat, "float"};
  float_decl.init->data = GLSLLiteralExpr{int64_t{1}};
  float_stmt->data = std::move(float_decl);

  auto float_stmt_2 = std::make_unique<GLSLStmt>();
  GLSLVarDeclStmt float_decl_2;
  float_decl_2.type = TypeRef{TokenKind::TypeFloat, "float"};
  float_decl_2.name = "b";
  float_decl_2.init = make_test_float_literal(0.0);
  float_stmt_2->data = std::move(float_decl_2);

  auto body = std::make_unique<GLSLStmt>();
  GLSLBlockStmt block;
  block.stmts.push_back(std::move(float_stmt));
  block.stmts.push_back(std::move(float_stmt_2));
  body->data = std::move(block);
  function.body = std::move(body);

  stage.declarations.push_back(GLSLDecl{std::move(function)});

  GLSLTextEmitter emitter;
  const std::string text = emitter.emit(stage);
  EXPECT_TRUE(contains(text, "float a = 1.0;")) << text;
  EXPECT_TRUE(contains(text, "float b = 0.0;")) << text;
}

TEST(Compiler, EnablingVulkanGLSLDoesNotChangeOpenGLStages) {
  static constexpr std::string_view source = R"axsl(
@version 450;

@in
interface Attributes {
    @location(0) vec3 position;
}

@uniform
interface Camera {
    mat4 projection;
}

@vertex
fn main(Attributes a, Camera camera) -> void {
    gl_Position = camera.projection * vec4(a.position, 1.0);
}
)axsl";

  Compiler compiler;

  auto without_vulkan = compiler.compile(source, "", "opengl-parity.axsl");
  ASSERT_TRUE(without_vulkan.ok()) << errors_str(without_vulkan);

  CompileOptions options;
  options.emit_vulkan_glsl = true;
  auto with_vulkan = compiler.compile(source, "", "opengl-parity.axsl", options);
  ASSERT_TRUE(with_vulkan.ok()) << errors_str(with_vulkan);

  EXPECT_EQ(without_vulkan.stages, with_vulkan.stages);
}

TEST(Compiler, VulkanGLSLRenamesBuiltins) {
  static constexpr std::string_view source = R"axsl(
@version 450;

@in
interface Attributes {
    @location(0) vec3 position;
}

@vertex
fn main(Attributes a) -> void {
    float builtins = float(gl_InstanceID + gl_VertexID);
    gl_Position = vec4(a.position + vec3(builtins), 1.0);
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_vulkan_glsl = true;

  auto result = compiler.compile(source, "", "vulkan-builtins.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_TRUE(result.vulkan_glsl_stages.count(StageKind::Vertex));

  const auto &vertex = result.vulkan_glsl_stages.at(StageKind::Vertex);
  EXPECT_TRUE(contains(vertex, "gl_InstanceIndex")) << vertex;
  EXPECT_TRUE(contains(vertex, "gl_VertexIndex")) << vertex;
  EXPECT_FALSE(contains(vertex, "gl_InstanceID")) << vertex;
  EXPECT_FALSE(contains(vertex, "gl_VertexID")) << vertex;
}

TEST(Compiler, VulkanGLSLStorageBlocksBecomeReadonlyBuffers) {
  static constexpr std::string_view source = R"axsl(
@version 450;

@std430
@binding(0)
in InstanceBuffer {
    mat4 models[];
} instance;

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(vec4(instance.models[0][0][0]));
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_vulkan_glsl = true;

  auto result =
      compiler.compile(source, "", "vulkan-storage-block.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_TRUE(result.vulkan_glsl_stages.count(StageKind::Fragment));

  const auto &fragment = result.vulkan_glsl_stages.at(StageKind::Fragment);
  EXPECT_TRUE(
      contains(
          fragment,
          "layout(set = 0, std430, binding = "
      )
  ) << fragment;
  EXPECT_TRUE(contains(fragment, "readonly buffer InstanceBuffer {")) << fragment;
}

TEST(Compiler, VulkanGLSLSamplerGlobalsPreserveSetAndBindingAnnotations) {
  static constexpr std::string_view source = R"axsl(
@version 450;

@set(2)
@binding(5)
uniform sampler2D albedo;

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main() -> FragmentOutput {
    return FragmentOutput(texture(albedo, vec2(0.5, 0.5)));
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_vulkan_glsl = true;

  auto result = compiler.compile(source, "", "vulkan-sampler-global.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &fragment = result.vulkan_glsl_stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(fragment, "layout(set = 2, binding = ")) << fragment;
  EXPECT_TRUE(contains(fragment, "uniform sampler2D albedo;")) << fragment;
}

TEST(Compiler, VulkanGLSLInjectsLocationsForInAndOutDeclarations) {
  GLSLStage stage;

  GLSLInterfaceBlockDecl input_block;
  input_block.storage = "in";
  input_block.block_name = "Attributes";
  input_block.fields.push_back(GLSLFieldDecl{
      .type = TypeRef{TokenKind::TypeVec3, "vec3"},
      .name = "position",
  });
  input_block.fields.push_back(GLSLFieldDecl{
      .type = TypeRef{TokenKind::TypeVec2, "vec2"},
      .name = "uv",
  });
  stage.declarations.push_back(GLSLDecl{std::move(input_block)});

  GLSLGlobalVarDecl input_global;
  input_global.storage = "in";
  input_global.type = TypeRef{TokenKind::TypeFloat, "float"};
  input_global.name = "instance_weight";
  stage.declarations.push_back(GLSLDecl{std::move(input_global)});

  GLSLInterfaceBlockDecl output_block;
  output_block.storage = "out";
  output_block.block_name = "Varyings";
  output_block.fields.push_back(GLSLFieldDecl{
      .type = TypeRef{TokenKind::TypeVec2, "vec2"},
      .name = "uv",
  });
  stage.declarations.push_back(GLSLDecl{std::move(output_block)});

  GLSLGlobalVarDecl output_global;
  output_global.storage = "out";
  output_global.type = TypeRef{TokenKind::TypeVec4, "vec4"};
  output_global.name = "color";
  stage.declarations.push_back(GLSLDecl{std::move(output_global)});

  annotate_vulkan_layouts(stage, ShaderPipelineLayout{}, StageKind::Vertex);

  GLSLTextEmitter emitter;
  const std::string text = emitter.emit(stage);
  EXPECT_TRUE(contains(text, "layout(location = 0) in Attributes {")) << text;
  EXPECT_TRUE(contains(text, "layout(location = 2) in float instance_weight;"))
      << text;
  EXPECT_TRUE(contains(text, "layout(location = 0) out Varyings {")) << text;
  EXPECT_TRUE(contains(text, "layout(location = 1) out vec4 color;")) << text;
}

TEST(Compiler, VulkanGLSLFragmentStageWrapsArrayBackedUniformValuesInBlocks) {
  static constexpr std::string_view source = R"axsl(
@version 450;

struct Material {
    sampler2D diffuse;
    float shininess;
};

struct PointLight {
    vec3 position;
    float intensity;
};

@uniform
interface Light {
    PointLight point_lights[4];
    Material materials[1];
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(Light light) -> FragmentOutput {
    vec4 sampled = texture(light.materials[0].diffuse, vec2(0.5, 0.5));
    return FragmentOutput(vec4(light.point_lights[0].position +
                               vec3(light.materials[0].shininess + sampled.r),
                               1.0));
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_vulkan_glsl = true;

  auto result =
      compiler.compile(source, "", "vulkan-fragment-uniform-blocks.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_TRUE(result.vulkan_glsl_stages.count(StageKind::Fragment));

  const auto &fragment = result.vulkan_glsl_stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(fragment, "PointLight _point_lights[4];")) << fragment;
  EXPECT_TRUE(contains(fragment, "float _materials_shininess[1];")) << fragment;
  EXPECT_TRUE(contains(fragment, "sampler2D _materials_diffuse[1];")) << fragment;
  EXPECT_TRUE(contains(fragment, "_materials_shininess[0]")) << fragment;
  EXPECT_TRUE(contains(fragment, "texture(_materials_diffuse[0],")) << fragment;
  EXPECT_FALSE(contains(fragment, "uniform PointLight _point_lights[4];"))
      << fragment;
  EXPECT_FALSE(contains(fragment, "uniform float _materials_shininess"))
      << fragment;
}

TEST(Compiler, VulkanGLSLStagesPadInactiveSharedUniformFields) {
  static constexpr std::string_view source = R"axsl(
@version 450;

struct Material {
    sampler2D diffuse;
    float shininess;
};

@in
interface Attributes {
    @location(0) vec3 position;
    @location(1) vec2 uv;
}

interface VertexOutput {
    vec2 uv;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@uniform
interface Entity {
    bool use_instancing = false;
    mat4 g_model;
    bool bloom_enabled = true;
    int bloom_layer = 0;
    int entity_id = 0;
}

@uniform
interface Light {
    mat4 light_space_matrix;
    Material materials[1];
}

@vertex
fn main(Entity entity, Light light, Attributes a) -> VertexOutput {
    VertexOutput output;
    output.uv = a.uv;
    gl_Position = light.light_space_matrix * entity.g_model * vec4(a.position, 1.0);
    if (entity.use_instancing) {
        gl_Position.x += 0.0;
    }
    return output;
}

@fragment
fn main(VertexOutput vertex, Entity entity, Light light) -> FragmentOutput {
    vec4 sampled = texture(light.materials[0].diffuse, vertex.uv);
    float bloom_gate =
        entity.bloom_enabled ? float(entity.bloom_layer + entity.entity_id) : 0.0;
    return FragmentOutput(
        vec4(sampled.rgb + vec3(light.materials[0].shininess + bloom_gate), 1.0));
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_vulkan_glsl = true;

  auto result = compiler.compile(
      source, "", "vulkan-shared-uniform-padding.axsl", options
  );
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_TRUE(result.vulkan_glsl_stages.count(StageKind::Vertex));
  ASSERT_TRUE(result.vulkan_glsl_stages.count(StageKind::Fragment));

  const auto &vertex = result.vulkan_glsl_stages.at(StageKind::Vertex);
  const auto &fragment = result.vulkan_glsl_stages.at(StageKind::Fragment);

  EXPECT_TRUE(contains(vertex, "bool _use_instancing;")) << vertex;
  EXPECT_TRUE(contains(vertex, "mat4 _g_model;")) << vertex;
  EXPECT_TRUE(contains(vertex, "bool _unused_entity_bloom_enabled;")) << vertex;
  EXPECT_TRUE(contains(vertex, "int _unused_entity_bloom_layer;")) << vertex;
  EXPECT_TRUE(contains(vertex, "int _unused_entity_entity_id;")) << vertex;

  EXPECT_TRUE(contains(fragment, "bool _unused_entity_use_instancing;"))
      << fragment;
  EXPECT_TRUE(contains(fragment, "mat4 _unused_entity_g_model;")) << fragment;
  EXPECT_TRUE(contains(fragment, "bool _bloom_enabled;")) << fragment;
  EXPECT_TRUE(contains(fragment, "int _bloom_layer;")) << fragment;
  EXPECT_TRUE(contains(fragment, "int _entity_id;")) << fragment;
  EXPECT_TRUE(contains(fragment, "mat4 _unused_light_light_space_matrix;"))
      << fragment;
  EXPECT_TRUE(contains(fragment, "float _materials_shininess[1];"))
      << fragment;

  EXPECT_LT(
      fragment.find("mat4 _unused_entity_g_model;"),
      fragment.find("bool _bloom_enabled;")
  ) << fragment;
  EXPECT_LT(
      fragment.find("mat4 _unused_light_light_space_matrix;"),
      fragment.find("float _materials_shininess[1];")
  ) << fragment;
}

TEST(Compiler, SharedLayoutStateKeepsSplitShaderBindingsDisjoint) {
  static constexpr std::string_view vertex_source = R"axsl(
@version 450;

@in
interface Mesh {
    @location(0) vec3 position;
    @location(2) vec2 texture_coordinates;
}

@uniform
interface Camera {
    @set(0) mat4 projection;
}

@uniform
interface Quad {
    @set(1) vec4 rect;
}

interface VertexOutput {
    vec2 texture_coordinates;
}

@vertex
fn main(Mesh mesh, Camera camera, Quad quad) -> VertexOutput {
    vec2 uv = vec2(mesh.texture_coordinates.x, 1.0 - mesh.texture_coordinates.y);
    vec2 world_position = quad.rect.xy + uv * quad.rect.zw;
    gl_Position = camera.projection * vec4(world_position, 0.0, 1.0);
    return VertexOutput(uv);
}
)axsl";

  static constexpr std::string_view fragment_source = R"axsl(
@version 450;

@uniform
interface Image {
    @set(1) sampler2D texture;
    @set(1) vec4 tint;
    @set(1) float sample_flip_y;
}

interface VertexOutput {
    vec2 texture_coordinates;
}

interface FragmentOutput {
    vec4 color;
}

@fragment
fn main(VertexOutput vertex, Image image) -> FragmentOutput {
    vec2 sample_uv = mix(
        vertex.texture_coordinates,
        vec2(vertex.texture_coordinates.x, 1.0 - vertex.texture_coordinates.y),
        clamp(image.sample_flip_y, 0.0, 1.0)
    );
    return FragmentOutput(texture(image.texture, sample_uv) * image.tint);
}
)axsl";

  using BindingLocation = std::pair<uint32_t, uint32_t>;

  auto collect_bindings = [](const CompileResult &result) {
    std::multiset<BindingLocation> bindings;

    for (const auto &block : result.merged_layout.resource_layout.value_blocks) {
      bindings.insert(
          {block.descriptor_set.value_or(0), block.binding.value_or(0)}
      );
    }
    for (const auto &resource : result.merged_layout.resource_layout.resources) {
      bindings.insert({resource.descriptor_set, resource.binding});
    }

    return bindings;
  };

  Compiler compiler;
  CompileOptions options;
  options.emit_vulkan_glsl = true;

  auto vertex_result = compiler.compile_with_shared_layout_state(
      vertex_source, "", "split-ui-quad.axsl", options
  );
  ASSERT_TRUE(vertex_result.ok()) << errors_str(vertex_result);

  auto fragment_result = compiler.compile_with_shared_layout_state(
      fragment_source, "", "split-ui-image.axsl", options
  );
  ASSERT_TRUE(fragment_result.ok()) << errors_str(fragment_result);

  EXPECT_EQ(
      collect_bindings(vertex_result),
      (std::multiset<BindingLocation>{{0u, 0u}, {1u, 0u}})
  );
  EXPECT_EQ(
      collect_bindings(fragment_result),
      (std::multiset<BindingLocation>{{1u, 1u}, {1u, 2u}})
  );

  const auto &fragment = fragment_result.vulkan_glsl_stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(fragment, "set = 1, binding = 1")) << fragment;
  EXPECT_TRUE(contains(fragment, "set = 1, std140, binding = 2")) << fragment;
}

TEST(Compiler, VulkanGLSLFragmentStageFlipsClipProjectedScreenUvY) {
  static constexpr std::string_view source = R"axsl(
@version 450;

@uniform
interface Camera {
    mat4 projection;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main(Camera camera) -> FragmentOutput {
    vec4 sample_clip = camera.projection * vec4(0.25, 0.5, 0.0, 1.0);
    vec2 sample_uv = (sample_clip.xy / sample_clip.w) * 0.5 + vec2(0.5);
    return FragmentOutput(vec4(sample_uv, 0.0, 1.0));
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_vulkan_glsl = true;

  auto result =
      compiler.compile(source, "", "vulkan-fragment-screen-uv.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_TRUE(result.vulkan_glsl_stages.count(StageKind::Fragment));

  const auto &fragment = result.vulkan_glsl_stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(fragment, "vec2 __astralix_vulkan_screen_uv(vec2 uv)"))
      << fragment;
  EXPECT_TRUE(
      contains(fragment, "vec2 sample_uv = __astralix_vulkan_screen_uv(")
  ) << fragment;
  EXPECT_TRUE(contains(fragment, "return vec2(uv.x, 1.0 - uv.y);"))
      << fragment;
}

TEST(Compiler, CompiledBuiltinCallsKeepFloatLiteralArgumentsFloatTyped) {
  Compiler compiler;
  auto src = make_fragment_program(R"axsl(
    float x = clamp(0.5, 0.0, 1.0);
    float y = max(x, 0.0);
    return FragmentOutput(vec4(x, y, 0.0, 1.0));
)axsl");

  auto result = compiler.compile(src);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(frag, "float x = clamp(0.5, 0.0, 1.0);")) << frag;
  EXPECT_TRUE(contains(frag, "float y = max(x, 0.0);")) << frag;
  EXPECT_TRUE(contains(frag, "_out_color = vec4(x, y, 0.0, 1.0);")) << frag;
}

TEST(Compiler, VulkanGLSLFragmentStageFlipsShadowProjectionY) {
  static constexpr std::string_view source = R"axsl(
@version 450;

@uniform
interface Light {
    sampler2D shadow_map;
}

interface FragmentOutput {
    @location(0) vec4 color;
}

fn get_shadow(sampler2D shadow_map, vec3 light_direction, vec3 normal, vec4 fragment_light_space) -> float {
    vec3 projection_coordinates = fragment_light_space.xyz / fragment_light_space.w;
    projection_coordinates = projection_coordinates * 0.5 + 0.5;
    float sampled_depth = texture(shadow_map, projection_coordinates.xy).r;
    return sampled_depth;
}

@fragment
fn main(Light light) -> FragmentOutput {
    float shadow = get_shadow(light.shadow_map, vec3(0.0, 1.0, 0.0), vec3(0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));
    return FragmentOutput(vec4(vec3(shadow), 1.0));
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_vulkan_glsl = true;

  auto result =
      compiler.compile(source, "", "vulkan-fragment-shadow-projection.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);
  ASSERT_TRUE(result.vulkan_glsl_stages.count(StageKind::Fragment));

  const auto &fragment = result.vulkan_glsl_stages.at(StageKind::Fragment);
  EXPECT_TRUE(contains(fragment, "projection_coordinates = (projection_coordinates * 0.5) + 0.5;"))
      << fragment;
  EXPECT_TRUE(contains(fragment, "projection_coordinates.y = 1.0 - projection_coordinates.y;"))
      << fragment;
}

TEST(Compiler, VulkanGLSLFragmentScreenUVPassIsNoOpWithoutPattern) {
  static constexpr std::string_view source = R"axsl(
@version 450;

interface FragmentOutput {
    @location(0) vec4 color;
}

@fragment
fn main() -> FragmentOutput {
    vec2 sample_uv = vec2(0.25, 0.5);
    return FragmentOutput(vec4(sample_uv, 0.0, 1.0));
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_vulkan_glsl = true;

  auto result = compiler.compile(source, "", "vulkan-fragment-no-screen-uv.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &fragment = result.vulkan_glsl_stages.at(StageKind::Fragment);
  EXPECT_FALSE(contains(fragment, "__astralix_vulkan_screen_uv")) << fragment;
}

TEST(Compiler, VulkanGLSLShadowProjectionPassIsNoOpWithoutGetShadow) {
  static constexpr std::string_view source = R"axsl(
@version 450;

interface FragmentOutput {
    @location(0) vec4 color;
}

fn sample_shadow(vec4 fragment_light_space) -> float {
    vec3 projection_coordinates = fragment_light_space.xyz / fragment_light_space.w;
    projection_coordinates = projection_coordinates * 0.5 + 0.5;
    return projection_coordinates.y;
}

@fragment
fn main() -> FragmentOutput {
    float shadow = sample_shadow(vec4(0.0, 0.0, 0.0, 1.0));
    return FragmentOutput(vec4(vec3(shadow), 1.0));
}
)axsl";

  Compiler compiler;
  CompileOptions options;
  options.emit_vulkan_glsl = true;

  auto result =
      compiler.compile(source, "", "vulkan-fragment-no-shadow-flip.axsl", options);
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &fragment = result.vulkan_glsl_stages.at(StageKind::Fragment);
  EXPECT_FALSE(contains(fragment, "projection_coordinates.y = 1 - projection_coordinates.y;"))
      << fragment;
}

TEST(Compiler, ComputeStageEmitsLocalSizeLayout) {
  static constexpr std::string_view source = R"axsl(
@version 450;

@compute(8, 4, 2)
fn main() -> void {
  barrier();
}
)axsl";

  Compiler compiler;
  auto result = compiler.compile(source, "", "compute-local-size.axsl");
  ASSERT_TRUE(result.ok()) << errors_str(result);

  const auto &compute = result.stages.at(StageKind::Compute);
  EXPECT_TRUE(
      contains(
          compute,
          "layout(local_size_x = 8, local_size_y = 4, local_size_z = 2) in;"
      )
  ) << compute;
}

} // namespace astralix
