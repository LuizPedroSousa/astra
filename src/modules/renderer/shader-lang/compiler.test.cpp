#include "shader-lang/compiler.hpp"
#include "shader-lang/artifacts/shader-artifact-pipeline.hpp"
#include "shader-lang/diagnostics.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

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
      contains(frag, "vec2 poisson_disk[2] = vec2[](vec2(0, 1), vec2(1, 0));"));
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
  EXPECT_TRUE(contains(result.stages.at(StageKind::Fragment), "? 1"));
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
  EXPECT_TRUE(contains(vert, "_stage_out.texture_coordinate = vec3(0);"));
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
  EXPECT_EQ(count_occurrences(frag, "_out_test = 0;"), 1u);
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
      contains(frag, "_out_color = vec4(input.texture_coordinate, 0, 1);"));
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
      vert, "gl_Position = (_projection * _view) * vec4(0, 0, 0, 1);"));
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
  EXPECT_TRUE(contains(vert, "uniform float global_value = 1"));

  const auto &frag = result.stages.at(StageKind::Fragment);
  EXPECT_FALSE(contains(frag, "uniform float global_value = 1"));
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

static std::string first_error(const CompileResult &r) {
  return r.errors.empty() ? "" : r.errors[0];
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

} // namespace astralix
