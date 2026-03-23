#include "shader-lang/artifacts/shader-artifact-pipeline.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <thread>

namespace astralix {
namespace {

std::string read_file_str(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void write_file(const std::filesystem::path &path, std::string_view content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << content;
}

void apply_plan(const ShaderArtifactPlan &plan) {
  for (const auto &path : plan.deletes) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }

  for (const auto &write : plan.writes) {
    std::filesystem::create_directories(write.path.parent_path());
    std::ofstream out(write.path, std::ios::binary | std::ios::trunc);
    out << write.content;
  }
}

std::filesystem::path make_temp_root(std::string_view suffix) {
  const auto root =
      std::filesystem::temp_directory_path() /
      std::filesystem::path("astralix-shader-artifacts-" + std::string(suffix));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "shaders");
  return root;
}

std::filesystem::path generated_header_path(const std::filesystem::path &root,
                                            std::string_view canonical_id) {
  return root / ".astralix" / "generated" / "shaders" /
         (sanitize_generated_shader_name(canonical_id) + ".hpp");
}

std::filesystem::path cache_metadata_path(const std::filesystem::path &root,
                                          std::string_view canonical_id) {
  return root / ".astralix" / "cache" / "shader-artifacts" /
         (sanitize_generated_shader_name(canonical_id) + ".meta");
}

std::filesystem::path reflection_path(const std::filesystem::path &root,
                                      std::string_view canonical_id) {
  return root / ".astralix" / "generated" / "reflections" /
         (sanitize_generated_shader_name(canonical_id) + ".reflection.json");
}

ShaderArtifactInput make_input(std::string_view canonical_id,
                               const std::filesystem::path &source_path,
                               const std::filesystem::path &output_root,
                               std::string_view umbrella_name = "project_shaders.hpp") {
  return {
      .canonical_id = std::string(canonical_id),
      .source_path = source_path,
      .output_root = output_root,
      .umbrella_name = std::string(umbrella_name),
  };
}

bool has_planned_write(const ShaderArtifactPlan &plan,
                       const std::filesystem::path &path) {
  return std::any_of(plan.writes.begin(), plan.writes.end(),
                     [&](const ShaderPlannedWrite &write) {
                       return write.path.lexically_normal() ==
                              path.lexically_normal();
                     });
}

bool has_planned_delete(const ShaderArtifactPlan &plan,
                        const std::filesystem::path &path) {
  return std::find(plan.deletes.begin(), plan.deletes.end(),
                   path.lexically_normal()) != plan.deletes.end();
}

constexpr std::string_view kSimpleShader = R"axsl(
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

} // namespace

TEST(ShaderArtifactPipeline, PlansBindingHeaderAndUmbrellaOutput) {
  const auto root = make_temp_root("plan");
  const auto shader_path = root / "shaders" / "light.axsl";
  write_file(shader_path, kSimpleShader);

  ShaderArtifactPipeline pipeline;
  auto plan =
      pipeline.build_plan({make_input("project/shaders/light.axsl", shader_path, root)});

  ASSERT_TRUE(plan.ok());
  EXPECT_EQ(plan.total_shaders, 1);
  EXPECT_EQ(plan.generated_shaders, 1);
  EXPECT_TRUE(has_planned_write(
      plan, generated_header_path(root, "project/shaders/light.axsl")));
  EXPECT_TRUE(has_planned_write(
      plan, cache_metadata_path(root, "project/shaders/light.axsl")));
  EXPECT_TRUE(has_planned_write(
      plan, root / ".astralix" / "generated" / "project_shaders.hpp"));

  std::filesystem::remove_all(root);
}

TEST(ShaderArtifactPipeline, UnchangedInputReusesCacheAndSkipsHeaderRewrite) {
  const auto root = make_temp_root("unchanged");
  const auto shader_path = root / "shaders" / "light.axsl";
  write_file(shader_path, kSimpleShader);

  ShaderArtifactPipeline pipeline;
  auto first =
      pipeline.build_plan({make_input("project/shaders/light.axsl", shader_path, root)});
  ASSERT_TRUE(first.ok());
  apply_plan(first);

  auto second =
      pipeline.build_plan({make_input("project/shaders/light.axsl", shader_path, root)});
  ASSERT_TRUE(second.ok());
  EXPECT_EQ(second.generated_shaders, 0);
  EXPECT_EQ(second.unchanged_shaders, 1);
  EXPECT_FALSE(has_planned_write(
      second, generated_header_path(root, "project/shaders/light.axsl")));

  std::filesystem::remove_all(root);
}

TEST(ShaderArtifactPipeline, IncludeChangesInvalidateDependentShader) {
  const auto root = make_temp_root("includes");
  const auto helper_path = root / "shaders" / "helper.axsl";
  const auto shader_path = root / "shaders" / "light.axsl";

  write_file(helper_path, "const float LIGHT_VALUE = 1.0;\n");
  write_file(shader_path, R"axsl(
@version 450;
@include "helper.axsl";

@uniform
interface Entity {
    float exposure = LIGHT_VALUE;
}

interface VertexOutput {
    @location(0) vec4 color;
}

@vertex
fn main(Entity entity) -> VertexOutput {
    return VertexOutput(vec4(entity.exposure));
}
)axsl");

  ShaderArtifactPipeline pipeline;
  auto first =
      pipeline.build_plan({make_input("project/shaders/light.axsl", shader_path, root)});
  ASSERT_TRUE(first.ok());
  apply_plan(first);

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  write_file(helper_path, "const float LIGHT_VALUE = 2.0;\n");

  auto second =
      pipeline.build_plan({make_input("project/shaders/light.axsl", shader_path, root)});
  ASSERT_TRUE(second.ok());
  EXPECT_EQ(second.generated_shaders, 1);

  std::filesystem::remove_all(root);
}

TEST(ShaderArtifactPipeline, FailedCompilePreservesLastKnownGoodOutputs) {
  const auto root = make_temp_root("failure");
  const auto shader_path = root / "shaders" / "light.axsl";
  write_file(shader_path, kSimpleShader);

  ShaderArtifactPipeline pipeline;
  auto first =
      pipeline.build_plan({make_input("project/shaders/light.axsl", shader_path, root)});
  ASSERT_TRUE(first.ok());
  apply_plan(first);

  const auto header_path =
      generated_header_path(root, "project/shaders/light.axsl");
  ASSERT_TRUE(std::filesystem::exists(header_path));

  write_file(shader_path, "@version 450;\n@vertex\nfn main(");
  auto second =
      pipeline.build_plan({make_input("project/shaders/light.axsl", shader_path, root)});
  EXPECT_FALSE(second.ok());
  EXPECT_FALSE(has_planned_write(second, header_path));
  EXPECT_FALSE(has_planned_delete(second, header_path));

  std::filesystem::remove_all(root);
}

TEST(ShaderArtifactPipeline, FailedCompileTracksMissingIncludeInWatchSet) {
  const auto root = make_temp_root("missing-include");
  const auto shader_path = root / "shaders" / "light.axsl";
  const auto missing_include = root / "shaders" / "missing.axsl";

  write_file(shader_path, R"axsl(
@version 450;
@include "missing.axsl";

interface VertexOutput {
    @location(0) vec4 color;
}

@vertex
fn main() -> VertexOutput {
    return VertexOutput(vec4(1.0));
}
)axsl");

  ShaderArtifactPipeline pipeline;
  auto plan =
      pipeline.build_plan({make_input("project/shaders/light.axsl", shader_path, root)});
  ASSERT_FALSE(plan.ok());
  EXPECT_NE(std::find(plan.watched_paths.begin(), plan.watched_paths.end(),
                      missing_include),
            plan.watched_paths.end());

  std::filesystem::remove_all(root);
}

TEST(ShaderArtifactPipeline, RemovesOrphanedOutputsWhenShaderIsDeletedFromSet) {
  const auto root = make_temp_root("remove");
  const auto shader_a = root / "shaders" / "a.axsl";
  const auto shader_b = root / "shaders" / "b.axsl";
  write_file(shader_a, kSimpleShader);
  write_file(shader_b, kSimpleShader);

  ShaderArtifactPipeline pipeline;
  auto first = pipeline.build_plan(
      {make_input("project/shaders/a.axsl", shader_a, root),
       make_input("project/shaders/b.axsl", shader_b, root)});
  ASSERT_TRUE(first.ok());
  apply_plan(first);

  auto second = pipeline.build_plan(
      {make_input("project/shaders/a.axsl", shader_a, root)});
  ASSERT_TRUE(second.ok());
  EXPECT_TRUE(has_planned_delete(
      second, generated_header_path(root, "project/shaders/b.axsl")));
  EXPECT_TRUE(has_planned_delete(
      second, cache_metadata_path(root, "project/shaders/b.axsl")));
  EXPECT_GT(second.planned_removals, 0);

  std::filesystem::remove_all(root);
}

TEST(ShaderArtifactPipeline, ProducesSeparateUmbrellasForDistinctRoots) {
  const auto project_root = make_temp_root("engine-project");
  const auto engine_root = make_temp_root("engine-root");
  const auto project_shader = project_root / "shaders" / "project.axsl";
  const auto engine_shader = engine_root / "shaders" / "engine.axsl";
  write_file(project_shader, kSimpleShader);
  write_file(engine_shader, kSimpleShader);

  ShaderArtifactPipeline pipeline;
  auto plan = pipeline.build_plan(
      {make_input("project/shaders/project.axsl", project_shader, project_root,
                  "project_shaders.hpp"),
       make_input("engine/shaders/engine.axsl", engine_shader, engine_root,
                  "engine_shaders.hpp")});

  ASSERT_TRUE(plan.ok());
  EXPECT_TRUE(has_planned_write(
      plan, project_root / ".astralix" / "generated" / "project_shaders.hpp"));
  EXPECT_TRUE(has_planned_write(
      plan, engine_root / ".astralix" / "generated" / "engine_shaders.hpp"));

  std::filesystem::remove_all(project_root);
  std::filesystem::remove_all(engine_root);
}

TEST(ShaderArtifactPipeline, ReflectionIrIsPlannedOnlyWhenRequested) {
  const auto root = make_temp_root("reflection");
  const auto shader_path = root / "shaders" / "light.axsl";
  write_file(shader_path, kSimpleShader);

  ShaderArtifactPipeline pipeline;
  auto default_plan =
      pipeline.build_plan({make_input("project/shaders/light.axsl", shader_path, root)});
  ASSERT_TRUE(default_plan.ok());
  EXPECT_FALSE(has_planned_write(
      default_plan, reflection_path(root, "project/shaders/light.axsl")));

  ShaderArtifactBuildOptions options;
  options.unit_artifacts.push_back(
      {.kind = ShaderUnitArtifactKind::ReflectionIR,
       .format = SerializationFormat::Json});
  auto reflection_plan = pipeline.build_plan(
      {make_input("project/shaders/light.axsl", shader_path, root)}, options);
  ASSERT_TRUE(reflection_plan.ok());
  EXPECT_TRUE(has_planned_write(
      reflection_plan, reflection_path(root, "project/shaders/light.axsl")));

  std::filesystem::remove_all(root);
}

} // namespace astralix
