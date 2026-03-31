#include "project-bootstrap.hpp"
#include "shader-lang/compiler.hpp"
#include "sync.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

namespace axgen {
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

std::filesystem::path make_temp_root(std::string_view suffix) {
  const auto root =
      std::filesystem::temp_directory_path() /
      std::filesystem::path("astralix-axgen-sync-" + std::string(suffix));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "assets" / "shaders");
  return root;
}

std::vector<astralix::ShaderArtifactInput>
to_artifact_inputs(const ProjectShaderDiscovery &discovery) {
  std::vector<astralix::ShaderArtifactInput> inputs;
  inputs.reserve(discovery.shaders.size());

  for (const auto &shader : discovery.shaders) {
    const bool is_engine = shader.canonical_id.starts_with("engine/");
    inputs.push_back({
        .canonical_id = shader.canonical_id,
        .source_path = shader.source_path,
        .output_root = is_engine ? discovery.engine_root : discovery.project_root,
        .umbrella_name = is_engine ? "engine_shaders.hpp"
                                   : "project_shaders.hpp",
    });
  }

  return inputs;
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

TEST(AxgenSync, DiscoversManifestShaderPathsAcrossProjectAndEngineRoots) {
  const auto root = make_temp_root("discovery");
  const auto local_shader = root / "assets" / "shaders" / "local.axsl";
  const auto manifest_path = root / "project.ax";

  write_file(local_shader, kSimpleShader);
  write_file(manifest_path, R"ax(
{
  "project": {
    "name": "Sandbox",
    "resources": {
      "directory": "assets"
    },
    "serialization": {
      "format": "json"
    }
  },
  "resources": [
    {
      "id": "shaders::local",
      "type": "Shader",
      "vertex": "@project/shaders/local.axsl",
      "fragment": "@project/shaders/local.axsl"
    },
    {
      "id": "shaders::engine",
      "type": "Shader",
      "vertex": "@engine/shaders/light.axsl",
      "fragment": "@engine/shaders/light.axsl"
    }
  ]
}
)ax");

  std::string error;
  auto discovery = discover_project_shaders(manifest_path, &error);
  ASSERT_TRUE(discovery.has_value()) << error;
  ASSERT_EQ(discovery->shaders.size(), 2u);

  EXPECT_EQ(discovery->shaders[0].canonical_id, "engine/shaders/light.axsl");
  EXPECT_EQ(discovery->shaders[0].source_path.lexically_normal(),
            (std::filesystem::path(ASTRALIX_ENGINE_ASSETS_DIR) / "shaders" /
             "light.axsl")
                .lexically_normal());
  EXPECT_EQ(discovery->shaders[1].canonical_id, "project/shaders/local.axsl");
  EXPECT_EQ(discovery->shaders[1].source_path.lexically_normal(),
            local_shader.lexically_normal());
  EXPECT_EQ(discovery->engine_root.lexically_normal(),
            std::filesystem::path(ASTRALIX_ENGINE_GENERATED_ROOT)
                .lexically_normal());

  std::filesystem::remove_all(root);
}

TEST(AxgenSync, DiscoversManifestShaderPathsFromYamlManifest) {
  const auto root = make_temp_root("yaml-discovery");
  const auto local_shader = root / "assets" / "shaders" / "local.axsl";
  const auto manifest_path = root / "project.ax";

  write_file(local_shader, kSimpleShader);
  write_file(manifest_path, R"ax(
project:
  name: Sandbox
  resources:
    directory: assets
  serialization:
    format: yaml
resources:
  - id: shaders::local
    type: Shader
    vertex: "@project/shaders/local.axsl"
    fragment: "@project/shaders/local.axsl"
  - id: shaders::engine
    type: Shader
    vertex: "@engine/shaders/light.axsl"
    fragment: "@engine/shaders/light.axsl"
)ax");

  std::string error;
  auto discovery = discover_project_shaders(manifest_path, &error);
  ASSERT_TRUE(discovery.has_value()) << error;
  ASSERT_EQ(discovery->shaders.size(), 2u);

  EXPECT_EQ(discovery->shaders[0].canonical_id, "engine/shaders/light.axsl");
  EXPECT_EQ(discovery->shaders[0].source_path.lexically_normal(),
            (std::filesystem::path(ASTRALIX_ENGINE_ASSETS_DIR) / "shaders" /
             "light.axsl")
                .lexically_normal());
  EXPECT_EQ(discovery->shaders[1].canonical_id, "project/shaders/local.axsl");
  EXPECT_EQ(discovery->shaders[1].source_path.lexically_normal(),
            local_shader.lexically_normal());

  std::filesystem::remove_all(root);
}

TEST(AxgenSync, DiscoversManifestShaderPathsFromTomlManifest) {
  const auto root = make_temp_root("toml-discovery");
  const auto local_shader = root / "assets" / "shaders" / "local.axsl";
  const auto manifest_path = root / "project.ax";

  write_file(local_shader, kSimpleShader);
  write_file(manifest_path, R"ax(
[project]
name = "Sandbox"

[project.resources]
directory = "assets"

[project.serialization]
format = "toml"

[[resources]]
id = "shaders::local"
type = "Shader"
vertex = "@project/shaders/local.axsl"
fragment = "@project/shaders/local.axsl"

[[resources]]
id = "shaders::engine"
type = "Shader"
vertex = "@engine/shaders/light.axsl"
fragment = "@engine/shaders/light.axsl"
)ax");

  std::string error;
  auto discovery = discover_project_shaders(manifest_path, &error);
  ASSERT_TRUE(discovery.has_value()) << error;
  ASSERT_EQ(discovery->shaders.size(), 2u);

  EXPECT_EQ(discovery->shaders[0].canonical_id, "engine/shaders/light.axsl");
  EXPECT_EQ(discovery->shaders[0].source_path.lexically_normal(),
            (std::filesystem::path(ASTRALIX_ENGINE_ASSETS_DIR) / "shaders" /
             "light.axsl")
                .lexically_normal());
  EXPECT_EQ(discovery->shaders[1].canonical_id, "project/shaders/local.axsl");
  EXPECT_EQ(discovery->shaders[1].source_path.lexically_normal(),
            local_shader.lexically_normal());

  std::filesystem::remove_all(root);
}

TEST(AxgenSync, AppliesArtifactPlanFromDiscoveredProjectShaders) {
  const auto root = make_temp_root("apply");
  const auto shader_path = root / "assets" / "shaders" / "light.axsl";
  const auto manifest_path = root / "project.ax";

  write_file(shader_path, kSimpleShader);
  write_file(manifest_path, R"ax(
{
  "project": {
    "name": "Sandbox",
    "resources": {
      "directory": "assets"
    },
    "serialization": {
      "format": "json"
    }
  },
  "resources": [
    {
      "id": "shaders::local",
      "type": "Shader",
      "vertex": "@project/shaders/light.axsl",
      "fragment": "@project/shaders/light.axsl"
    }
  ]
}
)ax");

  std::string error;
  auto discovery = discover_project_shaders(manifest_path, &error);
  ASSERT_TRUE(discovery.has_value()) << error;

  astralix::Compiler compiler;
  auto plan = compiler.build_artifact_plan(to_artifact_inputs(*discovery));
  ASSERT_TRUE(plan.ok());

  auto apply_result = apply_shader_artifact_plan(plan);
  ASSERT_TRUE(apply_result.ok());
  EXPECT_TRUE(std::filesystem::exists(
      root / ".astralix" / "generated" / "project_shaders.hpp"));
  EXPECT_TRUE(std::filesystem::exists(
      root / ".astralix" / "generated" / "shaders" /
          "project_shaders_light_axsl.hpp"));

  const auto umbrella =
      read_file_str(root / ".astralix" / "generated" / "project_shaders.hpp");
  EXPECT_NE(umbrella.find("#include \"shaders/project_shaders_light_axsl.hpp\""),
            std::string::npos);

  std::filesystem::remove_all(root);
}

TEST(AxgenSync, WritesEngineShaderArtifactsIntoEngineLocalOutputTree) {
  const auto root = make_temp_root("engine-output");
  const auto manifest_path = root / "project.ax";
  const auto engine_output_root =
      std::filesystem::path(ASTRALIX_ENGINE_GENERATED_ROOT);
  std::filesystem::remove_all(engine_output_root);

  write_file(manifest_path, R"ax(
{
  "project": {
    "name": "Sandbox",
    "resources": {
      "directory": "assets"
    },
    "serialization": {
      "format": "json"
    }
  },
  "resources": [
    {
      "id": "shaders::engine",
      "type": "Shader",
      "vertex": "@engine/shaders/light.axsl",
      "fragment": "@engine/shaders/light.axsl"
    }
  ]
}
)ax");

  std::string error;
  auto discovery = discover_project_shaders(manifest_path, &error);
  ASSERT_TRUE(discovery.has_value()) << error;

  astralix::Compiler compiler;
  auto plan = compiler.build_artifact_plan(to_artifact_inputs(*discovery));
  ASSERT_TRUE(plan.ok());

  auto apply_result = apply_shader_artifact_plan(plan);
  ASSERT_TRUE(apply_result.ok());
  EXPECT_TRUE(std::filesystem::exists(
      engine_output_root / ".astralix" / "generated" / "engine_shaders.hpp"));
  EXPECT_TRUE(std::filesystem::exists(
      engine_output_root / ".astralix" / "generated" / "shaders" /
          "engine_shaders_light_axsl.hpp"));

  const auto umbrella = read_file_str(
      engine_output_root / ".astralix" / "generated" / "engine_shaders.hpp");
  EXPECT_NE(umbrella.find("#include \"shaders/engine_shaders_light_axsl.hpp\""),
            std::string::npos);

  std::filesystem::remove_all(engine_output_root);
  std::filesystem::remove_all(root);
}

TEST(AxgenSync, FormatsSummaryWithRemovalAndFailureCounts) {
  astralix::ShaderArtifactPlan plan;
  plan.total_shaders = 2;
  plan.generated_shaders = 1;
  plan.unchanged_shaders = 1;
  plan.failed_shaders = 1;

  ArtifactPlanApplyResult apply_result;
  apply_result.removed_files = 3;
  apply_result.failures.push_back({"/tmp/out.hpp", "cannot write"});

  EXPECT_EQ(format_sync_summary(plan, apply_result),
            "axgen sync-shaders: 2 shader(s), 1 generated, 1 unchanged, 3 removed, 1 failed, 1 apply failed");
}

} // namespace axgen
