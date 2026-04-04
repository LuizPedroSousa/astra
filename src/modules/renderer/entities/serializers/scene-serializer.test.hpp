#include "axmesh-serializer.hpp"
#include "scene-serializer.hpp"

#include "arena.hpp"
#include "components/mesh.hpp"
#include "components/tags.hpp"
#include "components/transform.hpp"
#include "console.hpp"
#include "entities/scene-build-context.hpp"
#include "entities/scene.hpp"
#include "managers/project-manager.hpp"
#include "managers/scene-manager.hpp"
#include "project.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace astralix {
namespace {

constexpr std::string_view k_scene_id = "serializer";
constexpr std::string_view k_scene_type = "SceneSerializerTestScene";
constexpr std::string_view k_source_path = "scenes/source/serializer.axscene";
constexpr std::string_view k_preview_path =
    "scenes/preview/serializer.preview.axscene";
constexpr std::string_view k_runtime_path =
    "scenes/runtime/serializer.runtime.axscene";
constexpr std::string_view k_authored_entity_name = "serializer-authored";
constexpr std::string_view k_duplicate_entity_name = "serializer-duplicate";
constexpr std::string_view k_generated_entity_name = "serializer-generated";
constexpr std::string_view k_generated_override_name =
    "serializer-generated-overridden";
constexpr std::string_view k_suppressed_entity_name = "serializer-suppressed";
constexpr std::string_view k_editor_only_entity_name = "serializer-editor-only";

class SceneSerializerTestScene : public Scene {
public:
  SceneSerializerTestScene() : Scene(std::string(k_scene_type)) {}

  uint32_t preview_ready_calls() const { return m_preview_ready_calls; }
  uint32_t runtime_ready_calls() const { return m_runtime_ready_calls; }

protected:
  void build_source_world() override {
    auto authored = spawn_scene_entity(std::string(k_authored_entity_name));
    authored.emplace<rendering::Renderable>();
    authored.emplace<rendering::ShadowCaster>();
    authored.emplace<scene::Transform>(scene::Transform{
        .position = glm::vec3(1.0f, 2.0f, 3.0f),
        .scale = glm::vec3(4.0f, 5.0f, 6.0f),
        .rotation = glm::quat(1.0f, 0.25f, 0.5f, 0.75f),
        .matrix = glm::mat4(1.0f),
        .dirty = false,
    });
    authored.emplace<rendering::MeshSet>(rendering::MeshSet{
        .meshes = {make_test_mesh()},
    });

    auto duplicate = spawn_scene_entity(std::string(k_duplicate_entity_name));
    duplicate.emplace<scene::Transform>(scene::Transform{
        .position = glm::vec3(-1.0f, -2.0f, -3.0f),
        .scale = glm::vec3(1.0f),
        .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        .matrix = glm::mat4(1.0f),
        .dirty = false,
    });
    duplicate.emplace<rendering::MeshSet>(rendering::MeshSet{
        .meshes = {make_test_mesh()},
    });

    auto editor_only = spawn_scene_entity(std::string(k_editor_only_entity_name));
    editor_only.emplace<scene::EditorOnly>();
    editor_only.emplace<scene::Transform>(scene::Transform{
        .position = glm::vec3(10.0f, 0.0f, 0.0f),
        .scale = glm::vec3(1.0f),
        .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        .matrix = glm::mat4(1.0f),
        .dirty = false,
    });
  }

  void evaluate_build(SceneBuildContext &ctx) override {
    auto pass = ctx.begin_pass("serializer.generator");

    auto generated =
        pass.entity("generated-root", std::string(k_generated_entity_name));
    generated.component(rendering::ShadowCaster{});
    generated.component(scene::Transform{
        .position = glm::vec3(6.0f, 7.0f, 8.0f),
        .scale = glm::vec3(1.0f),
        .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        .matrix = glm::mat4(1.0f),
        .dirty = false,
    });

    auto suppressed =
        pass.entity("suppressed-root", std::string(k_suppressed_entity_name));
    suppressed.component(scene::Transform{
        .position = glm::vec3(2.0f, 3.0f, 4.0f),
        .scale = glm::vec3(1.0f),
        .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        .matrix = glm::mat4(1.0f),
        .dirty = false,
    });
  }

  void after_preview_ready() override { ++m_preview_ready_calls; }

  void after_runtime_ready() override { ++m_runtime_ready_calls; }

private:
  static Mesh make_test_mesh() {
    std::vector<Vertex> vertices = {
        {glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
         glm::vec2(0.0f, 0.0f)},
        {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
         glm::vec2(1.0f, 0.0f)},
        {glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
         glm::vec2(0.0f, 1.0f)},
    };

    return Mesh(std::move(vertices), {0, 1, 2});
  }

  uint32_t m_preview_ready_calls = 0u;
  uint32_t m_runtime_ready_calls = 0u;
};

std::filesystem::path test_project_directory() {
  return std::filesystem::temp_directory_path() /
         "astralix-scene-serializer-tests";
}

DerivedState make_derived_state() {
  DerivedState state;
  state.overrides.push_back(DerivedOverrideRecord{
      .key = DerivedEntityKey{
          .generator_id = "serializer.generator",
          .stable_key = "generated-root",
      },
      .active = true,
      .name = std::string(k_generated_override_name),
      .removed_components = {"ShadowCaster"},
      .components =
          {
              serialization::snapshot_component(scene::Transform{
                  .position = glm::vec3(12.0f, 3.0f, 5.0f),
                  .scale = glm::vec3(1.0f),
                  .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                  .matrix = glm::mat4(1.0f),
                  .dirty = false,
              }),
          },
  });
  state.suppressions.push_back(DerivedSuppressionRecord{
      .key = DerivedEntityKey{
          .generator_id = "serializer.generator",
          .stable_key = "suppressed-root",
      },
  });
  return state;
}

void initialize_test_project(
    SceneStartupTarget startup_target, bool clear_directory = true
) {
  const auto project_directory = test_project_directory();
  if (clear_directory) {
    std::filesystem::remove_all(project_directory);
  }
  std::filesystem::create_directories(project_directory);

  ConsoleManager::get().reset_for_testing();
  ProjectManager::init();
  SceneManager::init();

  ProjectConfig config;
  config.directory = project_directory.string();
  config.serialization.format = SerializationFormat::Json;
  config.scenes.startup = std::string(k_scene_id);
  config.scenes.startup_target = startup_target;
  config.scenes.entries.push_back(ProjectSceneEntryConfig{
      .id = std::string(k_scene_id),
      .type = std::string(k_scene_type),
      .source_path = std::string(k_source_path),
      .preview_path = std::string(k_preview_path),
      .runtime_path = std::string(k_runtime_path),
  });

  Ref<Project> project(new Project(config), [](Project *) {});
  ProjectManager::get()->add_project(project);
  SceneManager::get()->register_scene_type<SceneSerializerTestScene>(
      std::string(k_scene_type)
  );
}

Scene *activate_test_scene(
    SceneStartupTarget startup_target, bool clear_directory = true
) {
  initialize_test_project(startup_target, clear_directory);
  return SceneManager::get()->activate(std::string(k_scene_id));
}

int find_entity_index(ContextProxy entities_ctx, std::string_view name) {
  const size_t entity_count = entities_ctx.size();

  for (size_t index = 0; index < entity_count; ++index) {
    if (entities_ctx[static_cast<int>(index)]["name"].as<std::string>() ==
        name) {
      return static_cast<int>(index);
    }
  }

  return -1;
}

int find_component_index(ContextProxy entity_ctx, std::string_view type) {
  auto components_ctx = entity_ctx["components"];
  const size_t component_count = components_ctx.size();

  for (size_t index = 0; index < component_count; ++index) {
    if (components_ctx[static_cast<int>(index)]["type"].as<std::string>() ==
        type) {
      return static_cast<int>(index);
    }
  }

  return -1;
}

bool contains_string(ContextProxy &array_ctx, std::string_view value) {
  const size_t item_count = array_ctx.size();

  for (size_t index = 0; index < item_count; ++index) {
    if (array_ctx[static_cast<int>(index)].as<std::string>() == value) {
      return true;
    }
  }

  return false;
}

size_t count_axmesh_files(const std::filesystem::path &directory) {
  if (!std::filesystem::exists(directory)) {
    return 0u;
  }

  size_t mesh_file_count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().extension() == ".axmesh") {
      ++mesh_file_count;
    }
  }

  return mesh_file_count;
}

std::optional<EntityID>
find_world_entity_id(const ecs::World &world, std::string_view name) {
  std::optional<EntityID> entity_id;
  world.each<scene::SceneEntity>([&](
                                    EntityID candidate_id,
                                    const scene::SceneEntity &
                                ) {
    if (!entity_id.has_value() &&
        const_cast<ecs::World &>(world).entity(candidate_id).name() == name) {
      entity_id = candidate_id;
    }
  });
  return entity_id;
}

TEST(SceneSerializerTest, SavesSourceScenesWithDerivedOverridesAndSuppressions) {
  auto *scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->get_session_kind(), SceneSessionKind::Source);

  scene->set_derived_state(make_derived_state());
  scene->save_source();
  auto ctx = scene->get_serializer()->get_ctx();
  ASSERT_NE(ctx, nullptr);

  EXPECT_EQ((*ctx)["scene"]["version"].as<int>(), 5);
  EXPECT_EQ((*ctx)["scene"]["kind"].as<std::string>(), "source");
  EXPECT_EQ((*ctx)["scene"]["id"].as<std::string>(), k_scene_id);
  EXPECT_EQ((*ctx)["scene"]["type"].as<std::string>(), k_scene_type);

  const int authored_index =
      find_entity_index((*ctx)["entities"], k_authored_entity_name);
  const int editor_only_index =
      find_entity_index((*ctx)["entities"], k_editor_only_entity_name);
  ASSERT_GE(authored_index, 0);
  ASSERT_GE(editor_only_index, 0);
  EXPECT_EQ(find_entity_index((*ctx)["entities"], k_generated_entity_name), -1);
  EXPECT_EQ(find_entity_index((*ctx)["entities"], k_generated_override_name), -1);

  auto editor_tags = (*ctx)["entities"][editor_only_index]["tags"];
  ASSERT_EQ(editor_tags.kind(), SerializationTypeKind::Array);
  EXPECT_TRUE(contains_string(editor_tags, "EditorOnly"));

  ASSERT_EQ((*ctx)["derived_overrides"].size(), 1u);
  ASSERT_EQ((*ctx)["derived_suppressions"].size(), 1u);
  EXPECT_EQ((*ctx)["derived_overrides"][0]["generator_id"].as<std::string>(),
            "serializer.generator");
  EXPECT_EQ((*ctx)["derived_overrides"][0]["stable_key"].as<std::string>(),
            "generated-root");
  EXPECT_EQ((*ctx)["derived_overrides"][0]["name"].as<std::string>(),
            k_generated_override_name);
  EXPECT_EQ((*ctx)["derived_suppressions"][0]["stable_key"].as<std::string>(),
            "suppressed-root");

  const auto source_scene_path =
      test_project_directory() / std::filesystem::path(k_source_path);
  EXPECT_TRUE(std::filesystem::exists(source_scene_path));
}

TEST(SceneSerializerTest, PreviewBuildAppliesOverridesAndKeepsProvenance) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);

  source_scene->set_derived_state(make_derived_state());
  source_scene->save_source();
  source_scene->build_preview();

  const auto source_mesh_directory =
      test_project_directory() / "scenes" / "source" / "meshes";
  const auto preview_mesh_directory =
      test_project_directory() / "scenes" / "preview" / "preview-meshes";
  EXPECT_EQ(count_axmesh_files(source_mesh_directory), 1u);
  EXPECT_EQ(count_axmesh_files(preview_mesh_directory), 1u);

  auto *preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  ASSERT_NE(preview_scene, nullptr);
  ASSERT_EQ(preview_scene->get_session_kind(), SceneSessionKind::Preview);

  auto ctx = preview_scene->get_serializer()->get_ctx();
  ASSERT_NE(ctx, nullptr);
  EXPECT_EQ((*ctx)["scene"]["version"].as<int>(), 5);
  EXPECT_EQ((*ctx)["scene"]["kind"].as<std::string>(), "preview");
  EXPECT_GT((*ctx)["build"]["source_revision"].as<int>(), 0);
  EXPECT_EQ((*ctx)["build"]["built_at_utc"].kind(), SerializationTypeKind::String);
  EXPECT_EQ(find_entity_index((*ctx)["entities"], k_editor_only_entity_name), -1);
  EXPECT_EQ(find_entity_index((*ctx)["entities"], k_suppressed_entity_name), -1);

  const int generated_index =
      find_entity_index((*ctx)["entities"], k_generated_override_name);
  ASSERT_GE(generated_index, 0);

  auto generated_tags = (*ctx)["entities"][generated_index]["tags"];
  ASSERT_EQ(generated_tags.kind(), SerializationTypeKind::Array);
  EXPECT_TRUE(contains_string(generated_tags, "DerivedEntity"));

  const int meta_owner_index =
      find_component_index((*ctx)["entities"][generated_index], "MetaEntityOwner");
  ASSERT_GE(meta_owner_index, 0);
  auto owner_fields =
      (*ctx)["entities"][generated_index]["components"][meta_owner_index]["fields"];
  EXPECT_EQ(owner_fields["generator_id"].as<std::string>(), "serializer.generator");
  EXPECT_EQ(owner_fields["stable_key"].as<std::string>(), "generated-root");
  EXPECT_EQ(find_component_index((*ctx)["entities"][generated_index], "ShadowCaster"),
            -1);

  const int authored_index =
      find_entity_index((*ctx)["entities"], k_authored_entity_name);
  const int duplicate_index =
      find_entity_index((*ctx)["entities"], k_duplicate_entity_name);
  ASSERT_GE(authored_index, 0);
  ASSERT_GE(duplicate_index, 0);

  const int authored_mesh_index =
      find_component_index((*ctx)["entities"][authored_index], "MeshSet");
  const int duplicate_mesh_index =
      find_component_index((*ctx)["entities"][duplicate_index], "MeshSet");
  ASSERT_GE(authored_mesh_index, 0);
  ASSERT_GE(duplicate_mesh_index, 0);

  auto authored_fields =
      (*ctx)["entities"][authored_index]["components"][authored_mesh_index]["fields"];
  auto duplicate_fields =
      (*ctx)["entities"][duplicate_index]["components"][duplicate_mesh_index]["fields"];

  EXPECT_EQ(authored_fields["asset"]["path"].kind(), SerializationTypeKind::String);
  EXPECT_EQ(authored_fields["asset"]["path"].as<std::string>(),
            duplicate_fields["asset"]["path"].as<std::string>());
  EXPECT_TRUE(authored_fields["asset"]["path"]
                  .as<std::string>()
                  .starts_with("scenes/preview/preview-meshes/"));
}

TEST(SceneSerializerTest, RuntimePromotionStripsPreviewOnlyProvenance) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);

  source_scene->set_derived_state(make_derived_state());
  source_scene->save_source();
  source_scene->build_preview();

  auto *preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  ASSERT_NE(preview_scene, nullptr);
  ASSERT_TRUE(preview_scene->promote_preview_to_runtime());

  const auto runtime_mesh_directory =
      test_project_directory() / "scenes" / "runtime" / "meshes";
  EXPECT_EQ(count_axmesh_files(runtime_mesh_directory), 1u);

  auto *runtime_scene =
      SceneManager::get()->activate_runtime(std::string(k_scene_id));
  ASSERT_NE(runtime_scene, nullptr);
  ASSERT_EQ(runtime_scene->get_session_kind(), SceneSessionKind::Runtime);

  auto ctx = runtime_scene->get_serializer()->get_ctx();
  ASSERT_NE(ctx, nullptr);
  EXPECT_EQ((*ctx)["scene"]["version"].as<int>(), 5);
  EXPECT_EQ((*ctx)["scene"]["kind"].as<std::string>(), "runtime");
  EXPECT_EQ((*ctx)["build"]["promoted_at_utc"].kind(), SerializationTypeKind::String);
  EXPECT_EQ(find_entity_index((*ctx)["entities"], k_suppressed_entity_name), -1);

  const int generated_index =
      find_entity_index((*ctx)["entities"], k_generated_override_name);
  ASSERT_GE(generated_index, 0);
  EXPECT_EQ(find_component_index((*ctx)["entities"][generated_index], "MetaEntityOwner"),
            -1);
  auto generated_tags = (*ctx)["entities"][generated_index]["tags"];
  ASSERT_EQ(generated_tags.kind(), SerializationTypeKind::Array);
  EXPECT_FALSE(contains_string(generated_tags, "DerivedEntity"));
}

TEST(SceneSerializerTest, SourceOverlayEditsOnDerivedEntitiesCarryIntoPreview) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);

  const auto generated_entity_id =
      find_world_entity_id(source_scene->world(), k_generated_entity_name);
  ASSERT_TRUE(generated_entity_id.has_value());

  auto generated_entity = source_scene->world().entity(*generated_entity_id);
  generated_entity.set_name("serializer-generated-source-edit");
  auto *generated_transform = generated_entity.get<scene::Transform>();
  ASSERT_NE(generated_transform, nullptr);
  generated_transform->position = glm::vec3(42.0f, 24.0f, 12.0f);
  source_scene->world().touch();

  source_scene->build_preview();

  auto *preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  ASSERT_NE(preview_scene, nullptr);

  const auto edited_entity_id =
      find_world_entity_id(preview_scene->world(), "serializer-generated-source-edit");
  ASSERT_TRUE(edited_entity_id.has_value());

  auto edited_entity = preview_scene->world().entity(*edited_entity_id);
  auto *edited_transform = edited_entity.get<scene::Transform>();
  ASSERT_NE(edited_transform, nullptr);
  EXPECT_FLOAT_EQ(edited_transform->position.x, 42.0f);
  EXPECT_FLOAT_EQ(edited_transform->position.y, 24.0f);
  EXPECT_FLOAT_EQ(edited_transform->position.z, 12.0f);
}

TEST(SceneSerializerTest, PreviewActivationDoesNotPromoteSourceAutomatically) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);

  const auto preview_scene_path =
      test_project_directory() / std::filesystem::path(k_preview_path);
  EXPECT_FALSE(std::filesystem::exists(preview_scene_path));

  const auto authored_entity_id =
      find_world_entity_id(source_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(authored_entity_id.has_value());
  auto authored_entity = source_scene->world().entity(*authored_entity_id);
  auto *authored_transform = authored_entity.get<scene::Transform>();
  ASSERT_NE(authored_transform, nullptr);
  authored_transform->position.x = 19.0f;
  source_scene->world().touch();

  auto *preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  EXPECT_EQ(preview_scene, nullptr);
  EXPECT_FALSE(std::filesystem::exists(preview_scene_path));

  const auto active_kind = SceneManager::get()->get_active_scene_session_kind();
  ASSERT_TRUE(active_kind.has_value());
  EXPECT_EQ(*active_kind, SceneSessionKind::Source);
}

TEST(SceneSerializerTest, PromoteSourceStagesPreviewInMemoryUntilExplicitSave) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);

  const auto preview_scene_path =
      test_project_directory() / std::filesystem::path(k_preview_path);
  const auto runtime_scene_path =
      test_project_directory() / std::filesystem::path(k_runtime_path);
  EXPECT_FALSE(std::filesystem::exists(preview_scene_path));

  const auto authored_entity_id =
      find_world_entity_id(source_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(authored_entity_id.has_value());
  auto authored_entity = source_scene->world().entity(*authored_entity_id);
  auto *authored_transform = authored_entity.get<scene::Transform>();
  ASSERT_NE(authored_transform, nullptr);
  authored_transform->position.x = 19.0f;
  source_scene->world().touch();

  ASSERT_TRUE(
      SceneManager::get()->promote_source_to_preview(std::string(k_scene_id))
  );
  EXPECT_FALSE(std::filesystem::exists(preview_scene_path));
  EXPECT_FALSE(std::filesystem::exists(runtime_scene_path));

  const auto status =
      SceneManager::get()->get_scene_lifecycle_status(std::string(k_scene_id));
  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(status->preview, ScenePreviewState::Current);
  EXPECT_EQ(status->runtime, SceneRuntimeState::Missing);

  auto *preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  ASSERT_NE(preview_scene, nullptr);
  EXPECT_FALSE(std::filesystem::exists(preview_scene_path));

  const auto preview_status =
      SceneManager::get()->get_scene_lifecycle_status(std::string(k_scene_id));
  ASSERT_TRUE(preview_status.has_value());
  EXPECT_EQ(preview_status->runtime, SceneRuntimeState::Missing);
  EXPECT_EQ(
      SceneManager::get()->activate_runtime(std::string(k_scene_id)),
      nullptr
  );

  const auto preview_authored_entity_id =
      find_world_entity_id(preview_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(preview_authored_entity_id.has_value());
  auto preview_authored_entity =
      preview_scene->world().entity(*preview_authored_entity_id);
  auto *preview_transform =
      preview_authored_entity.get<scene::Transform>();
  ASSERT_NE(preview_transform, nullptr);
  EXPECT_FLOAT_EQ(preview_transform->position.x, 19.0f);

  ASSERT_TRUE(SceneManager::get()->promote_preview(std::string(k_scene_id)));
  EXPECT_FALSE(std::filesystem::exists(runtime_scene_path));
  EXPECT_FALSE(std::filesystem::exists(preview_scene_path));

  preview_scene->save_preview();
  EXPECT_TRUE(std::filesystem::exists(preview_scene_path));

  auto *runtime_scene =
      SceneManager::get()->activate_runtime(std::string(k_scene_id));
  ASSERT_NE(runtime_scene, nullptr);
  const auto runtime_authored_entity_id =
      find_world_entity_id(runtime_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(runtime_authored_entity_id.has_value());
  auto runtime_authored_entity =
      runtime_scene->world().entity(*runtime_authored_entity_id);
  auto *runtime_transform =
      runtime_authored_entity.get<scene::Transform>();
  ASSERT_NE(runtime_transform, nullptr);
  EXPECT_FLOAT_EQ(runtime_transform->position.x, 19.0f);

  runtime_scene->save_runtime();
  EXPECT_TRUE(std::filesystem::exists(runtime_scene_path));
}

TEST(SceneSerializerTest, PreviewAndRuntimeSessionsCanPlayPauseAndStop) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);

  const auto source_execution_state =
      SceneManager::get()->get_active_scene_execution_state();
  ASSERT_TRUE(source_execution_state.has_value());
  EXPECT_EQ(*source_execution_state, SceneExecutionState::Static);

  ASSERT_TRUE(
      SceneManager::get()->promote_source_to_preview(std::string(k_scene_id))
  );

  auto *preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  ASSERT_NE(preview_scene, nullptr);

  auto preview_execution_state =
      SceneManager::get()->get_active_scene_execution_state();
  ASSERT_TRUE(preview_execution_state.has_value());
  EXPECT_EQ(*preview_execution_state, SceneExecutionState::Stopped);

  ASSERT_TRUE(SceneManager::get()->pause_active_scene());
  preview_execution_state =
      SceneManager::get()->get_active_scene_execution_state();
  ASSERT_TRUE(preview_execution_state.has_value());
  EXPECT_EQ(*preview_execution_state, SceneExecutionState::Paused);

  ASSERT_TRUE(SceneManager::get()->play_active_scene());
  preview_execution_state =
      SceneManager::get()->get_active_scene_execution_state();
  ASSERT_TRUE(preview_execution_state.has_value());
  EXPECT_EQ(*preview_execution_state, SceneExecutionState::Playing);

  const auto preview_entity_id =
      find_world_entity_id(preview_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(preview_entity_id.has_value());
  auto preview_entity = preview_scene->world().entity(*preview_entity_id);
  auto *preview_transform = preview_entity.get<scene::Transform>();
  ASSERT_NE(preview_transform, nullptr);
  preview_transform->position.x = 91.0f;
  preview_scene->world().touch();

  ASSERT_TRUE(SceneManager::get()->stop_active_scene());
  preview_execution_state =
      SceneManager::get()->get_active_scene_execution_state();
  ASSERT_TRUE(preview_execution_state.has_value());
  EXPECT_EQ(*preview_execution_state, SceneExecutionState::Stopped);
  EXPECT_TRUE(SceneManager::get()->flush_pending_active_scene_state());

  const auto reset_preview_entity_id =
      find_world_entity_id(preview_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(reset_preview_entity_id.has_value());
  auto reset_preview_entity =
      preview_scene->world().entity(*reset_preview_entity_id);
  auto *reset_preview_transform =
      reset_preview_entity.get<scene::Transform>();
  ASSERT_NE(reset_preview_transform, nullptr);
  EXPECT_FLOAT_EQ(reset_preview_transform->position.x, 1.0f);

  ASSERT_TRUE(SceneManager::get()->promote_preview(std::string(k_scene_id)));

  ASSERT_TRUE(SceneManager::get()->play_active_scene());
  ASSERT_TRUE(SceneManager::get()->stop_active_scene());
  preview_execution_state =
      SceneManager::get()->get_active_scene_execution_state();
  ASSERT_TRUE(preview_execution_state.has_value());
  EXPECT_EQ(*preview_execution_state, SceneExecutionState::Stopped);
  EXPECT_TRUE(SceneManager::get()->flush_pending_active_scene_state());

  const auto promoted_preview_entity_id =
      find_world_entity_id(preview_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(promoted_preview_entity_id.has_value());
  auto promoted_preview_entity =
      preview_scene->world().entity(*promoted_preview_entity_id);
  auto *promoted_preview_transform =
      promoted_preview_entity.get<scene::Transform>();
  ASSERT_NE(promoted_preview_transform, nullptr);
  EXPECT_FLOAT_EQ(promoted_preview_transform->position.x, 1.0f);

  auto *runtime_scene =
      SceneManager::get()->activate_runtime(std::string(k_scene_id));
  ASSERT_NE(runtime_scene, nullptr);

  auto runtime_execution_state =
      SceneManager::get()->get_active_scene_execution_state();
  ASSERT_TRUE(runtime_execution_state.has_value());
  EXPECT_EQ(*runtime_execution_state, SceneExecutionState::Stopped);

  ASSERT_TRUE(SceneManager::get()->pause_active_scene());
  runtime_execution_state =
      SceneManager::get()->get_active_scene_execution_state();
  ASSERT_TRUE(runtime_execution_state.has_value());
  EXPECT_EQ(*runtime_execution_state, SceneExecutionState::Paused);

  const auto runtime_entity_id =
      find_world_entity_id(runtime_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(runtime_entity_id.has_value());
  auto runtime_entity = runtime_scene->world().entity(*runtime_entity_id);
  auto *runtime_transform = runtime_entity.get<scene::Transform>();
  ASSERT_NE(runtime_transform, nullptr);
  runtime_transform->position.x = 55.0f;
  runtime_scene->world().touch();

  ASSERT_TRUE(SceneManager::get()->stop_active_scene());
  runtime_execution_state =
      SceneManager::get()->get_active_scene_execution_state();
  ASSERT_TRUE(runtime_execution_state.has_value());
  EXPECT_EQ(*runtime_execution_state, SceneExecutionState::Stopped);
  EXPECT_TRUE(SceneManager::get()->flush_pending_active_scene_state());

  const auto reset_runtime_entity_id =
      find_world_entity_id(runtime_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(reset_runtime_entity_id.has_value());
  auto reset_runtime_entity =
      runtime_scene->world().entity(*reset_runtime_entity_id);
  auto *reset_runtime_transform =
      reset_runtime_entity.get<scene::Transform>();
  ASSERT_NE(reset_runtime_transform, nullptr);
  EXPECT_FLOAT_EQ(reset_runtime_transform->position.x, 1.0f);

  ASSERT_TRUE(SceneManager::get()->play_active_scene());
  ASSERT_TRUE(SceneManager::get()->stop_active_scene());
  runtime_execution_state =
      SceneManager::get()->get_active_scene_execution_state();
  ASSERT_TRUE(runtime_execution_state.has_value());
  EXPECT_EQ(*runtime_execution_state, SceneExecutionState::Stopped);
  EXPECT_TRUE(SceneManager::get()->flush_pending_active_scene_state());

  const auto repeated_reset_runtime_entity_id =
      find_world_entity_id(runtime_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(repeated_reset_runtime_entity_id.has_value());
  auto repeated_reset_runtime_entity =
      runtime_scene->world().entity(*repeated_reset_runtime_entity_id);
  auto *repeated_reset_runtime_transform =
      repeated_reset_runtime_entity.get<scene::Transform>();
  ASSERT_NE(repeated_reset_runtime_transform, nullptr);
  EXPECT_FLOAT_EQ(repeated_reset_runtime_transform->position.x, 1.0f);
}

TEST(SceneSerializerTest, RuntimeStopResetReplaysReadyHook) {
  auto *source_scene = static_cast<SceneSerializerTestScene *>(
      activate_test_scene(SceneStartupTarget::Source)
  );
  ASSERT_NE(source_scene, nullptr);

  ASSERT_TRUE(SceneManager::get()->build_preview(std::string(k_scene_id)));
  ASSERT_TRUE(SceneManager::get()->promote_preview(std::string(k_scene_id)));

  auto *runtime_scene = static_cast<SceneSerializerTestScene *>(
      SceneManager::get()->activate_runtime(std::string(k_scene_id))
  );
  ASSERT_NE(runtime_scene, nullptr);
  EXPECT_EQ(runtime_scene->runtime_ready_calls(), 1u);

  ASSERT_TRUE(SceneManager::get()->stop_active_scene());
  EXPECT_TRUE(SceneManager::get()->flush_pending_active_scene_state());
  EXPECT_EQ(runtime_scene->runtime_ready_calls(), 2u);
}

TEST(SceneSerializerTest, RuntimeArtifactReloadReplaysReadyHook) {
  auto *source_scene = static_cast<SceneSerializerTestScene *>(
      activate_test_scene(SceneStartupTarget::Source)
  );
  ASSERT_NE(source_scene, nullptr);

  ASSERT_TRUE(SceneManager::get()->build_preview(std::string(k_scene_id)));
  ASSERT_TRUE(SceneManager::get()->promote_preview(std::string(k_scene_id)));

  auto *runtime_scene = static_cast<SceneSerializerTestScene *>(
      SceneManager::get()->activate_runtime(std::string(k_scene_id))
  );
  ASSERT_NE(runtime_scene, nullptr);
  runtime_scene->save_runtime();

  const auto runtime_scene_path =
      test_project_directory() / std::filesystem::path(k_runtime_path);
  ASSERT_TRUE(std::filesystem::exists(runtime_scene_path));
  EXPECT_EQ(runtime_scene->runtime_ready_calls(), 1u);

  const auto current_write_time =
      std::filesystem::last_write_time(runtime_scene_path);
  std::filesystem::last_write_time(
      runtime_scene_path, current_write_time + std::chrono::seconds(1)
  );

  auto *reloaded_runtime_scene = static_cast<SceneSerializerTestScene *>(
      SceneManager::get()->activate_runtime(std::string(k_scene_id))
  );
  ASSERT_EQ(reloaded_runtime_scene, runtime_scene);
  EXPECT_EQ(runtime_scene->runtime_ready_calls(), 2u);
}

TEST(SceneSerializerTest, PromotePreviewKeepsRuntimeActiveAndReloadsRuntime) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);

  const auto authored_entity_id =
      find_world_entity_id(source_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(authored_entity_id.has_value());

  auto authored_entity = source_scene->world().entity(*authored_entity_id);
  auto *authored_transform = authored_entity.get<scene::Transform>();
  ASSERT_NE(authored_transform, nullptr);
  authored_transform->position.x = 11.0f;
  source_scene->world().touch();

  ASSERT_TRUE(SceneManager::get()->build_preview(std::string(k_scene_id)));
  ASSERT_TRUE(SceneManager::get()->promote_preview(std::string(k_scene_id)));

  auto *runtime_scene =
      SceneManager::get()->activate_runtime(std::string(k_scene_id));
  ASSERT_NE(runtime_scene, nullptr);
  const auto active_runtime_kind =
      SceneManager::get()->get_active_scene_session_kind();
  ASSERT_TRUE(active_runtime_kind.has_value());
  EXPECT_EQ(*active_runtime_kind, SceneSessionKind::Runtime);

  const auto runtime_authored_entity_id =
      find_world_entity_id(runtime_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(runtime_authored_entity_id.has_value());
  auto runtime_authored_entity =
      runtime_scene->world().entity(*runtime_authored_entity_id);
  auto *runtime_transform =
      runtime_authored_entity.get<scene::Transform>();
  ASSERT_NE(runtime_transform, nullptr);
  EXPECT_FLOAT_EQ(runtime_transform->position.x, 11.0f);

  auto *reloaded_source_scene =
      SceneManager::get()->activate_source(std::string(k_scene_id));
  ASSERT_NE(reloaded_source_scene, nullptr);
  const auto reloaded_authored_entity_id =
      find_world_entity_id(reloaded_source_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(reloaded_authored_entity_id.has_value());
  auto reloaded_authored_entity =
      reloaded_source_scene->world().entity(*reloaded_authored_entity_id);
  auto *reloaded_authored_transform =
      reloaded_authored_entity.get<scene::Transform>();
  ASSERT_NE(reloaded_authored_transform, nullptr);
  reloaded_authored_transform->position.x = 27.0f;
  reloaded_source_scene->world().touch();

  ASSERT_TRUE(SceneManager::get()->build_preview(std::string(k_scene_id)));
  ASSERT_NE(
      SceneManager::get()->activate_runtime(std::string(k_scene_id)),
      nullptr
  );
  ASSERT_TRUE(SceneManager::get()->promote_preview(std::string(k_scene_id)));
  const auto reloaded_runtime_kind =
      SceneManager::get()->get_active_scene_session_kind();
  ASSERT_TRUE(reloaded_runtime_kind.has_value());
  EXPECT_EQ(*reloaded_runtime_kind, SceneSessionKind::Runtime);

  auto *active_runtime_scene = SceneManager::get()->get_active_scene();
  ASSERT_NE(active_runtime_scene, nullptr);
  const auto updated_runtime_entity_id =
      find_world_entity_id(active_runtime_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(updated_runtime_entity_id.has_value());
  auto updated_runtime_entity =
      active_runtime_scene->world().entity(*updated_runtime_entity_id);
  auto *updated_runtime_transform =
      updated_runtime_entity.get<scene::Transform>();
  ASSERT_NE(updated_runtime_transform, nullptr);
  EXPECT_FLOAT_EQ(updated_runtime_transform->position.x, 27.0f);
}

TEST(SceneSerializerTest, PromotePreviewUsesCurrentPreviewSessionState) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);
  ASSERT_TRUE(SceneManager::get()->build_preview(std::string(k_scene_id)));

  auto *preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  ASSERT_NE(preview_scene, nullptr);

  const auto preview_authored_entity_id =
      find_world_entity_id(preview_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(preview_authored_entity_id.has_value());
  auto preview_authored_entity =
      preview_scene->world().entity(*preview_authored_entity_id);
  auto *preview_transform =
      preview_authored_entity.get<scene::Transform>();
  ASSERT_NE(preview_transform, nullptr);
  preview_transform->position.x = 31.0f;
  preview_scene->world().touch();

  ASSERT_TRUE(SceneManager::get()->promote_preview(std::string(k_scene_id)));

  auto *runtime_scene =
      SceneManager::get()->activate_runtime(std::string(k_scene_id));
  ASSERT_NE(runtime_scene, nullptr);
  const auto runtime_authored_entity_id =
      find_world_entity_id(runtime_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(runtime_authored_entity_id.has_value());
  auto runtime_authored_entity =
      runtime_scene->world().entity(*runtime_authored_entity_id);
  auto *runtime_transform =
      runtime_authored_entity.get<scene::Transform>();
  ASSERT_NE(runtime_transform, nullptr);
  EXPECT_FLOAT_EQ(runtime_transform->position.x, 31.0f);
}

TEST(SceneSerializerTest, RuntimeCanReturnToCurrentPreviewSession) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);
  ASSERT_TRUE(
      SceneManager::get()->promote_source_to_preview(std::string(k_scene_id))
  );

  auto *preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  ASSERT_NE(preview_scene, nullptr);

  const auto preview_authored_entity_id =
      find_world_entity_id(preview_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(preview_authored_entity_id.has_value());
  auto preview_authored_entity =
      preview_scene->world().entity(*preview_authored_entity_id);
  auto *preview_transform =
      preview_authored_entity.get<scene::Transform>();
  ASSERT_NE(preview_transform, nullptr);
  preview_transform->position.x = 47.0f;
  preview_scene->world().touch();

  ASSERT_TRUE(SceneManager::get()->promote_preview(std::string(k_scene_id)));

  auto *runtime_scene =
      SceneManager::get()->activate_runtime(std::string(k_scene_id));
  ASSERT_NE(runtime_scene, nullptr);

  auto *returned_preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  ASSERT_NE(returned_preview_scene, nullptr);

  const auto returned_preview_entity_id =
      find_world_entity_id(returned_preview_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(returned_preview_entity_id.has_value());
  auto returned_preview_entity =
      returned_preview_scene->world().entity(*returned_preview_entity_id);
  auto *returned_preview_transform =
      returned_preview_entity.get<scene::Transform>();
  ASSERT_NE(returned_preview_transform, nullptr);
  EXPECT_FLOAT_EQ(returned_preview_transform->position.x, 47.0f);

  const auto active_kind = SceneManager::get()->get_active_scene_session_kind();
  ASSERT_TRUE(active_kind.has_value());
  EXPECT_EQ(*active_kind, SceneSessionKind::Preview);
}

TEST(SceneSerializerTest, RuntimeRemainsAvailableAfterPreviewAdvancesInMemory) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);
  ASSERT_TRUE(
      SceneManager::get()->promote_source_to_preview(std::string(k_scene_id))
  );

  auto *preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  ASSERT_NE(preview_scene, nullptr);
  ASSERT_TRUE(SceneManager::get()->promote_preview(std::string(k_scene_id)));

  auto *runtime_scene =
      SceneManager::get()->activate_runtime(std::string(k_scene_id));
  ASSERT_NE(runtime_scene, nullptr);
  const auto initial_runtime_entity_id =
      find_world_entity_id(runtime_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(initial_runtime_entity_id.has_value());
  auto initial_runtime_entity =
      runtime_scene->world().entity(*initial_runtime_entity_id);
  auto *initial_runtime_transform =
      initial_runtime_entity.get<scene::Transform>();
  ASSERT_NE(initial_runtime_transform, nullptr);
  EXPECT_FLOAT_EQ(initial_runtime_transform->position.x, 1.0f);

  const auto authored_entity_id =
      find_world_entity_id(source_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(authored_entity_id.has_value());
  auto authored_entity = source_scene->world().entity(*authored_entity_id);
  auto *authored_transform = authored_entity.get<scene::Transform>();
  ASSERT_NE(authored_transform, nullptr);
  authored_transform->position.x = 63.0f;
  source_scene->world().touch();

  ASSERT_TRUE(
      SceneManager::get()->promote_source_to_preview(std::string(k_scene_id))
  );

  const auto status =
      SceneManager::get()->get_scene_lifecycle_status(std::string(k_scene_id));
  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(status->preview, ScenePreviewState::Current);
  EXPECT_EQ(status->runtime, SceneRuntimeState::BehindPreview);

  runtime_scene =
      SceneManager::get()->activate_runtime(std::string(k_scene_id));
  ASSERT_NE(runtime_scene, nullptr);
  const auto returned_runtime_entity_id =
      find_world_entity_id(runtime_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(returned_runtime_entity_id.has_value());
  auto returned_runtime_entity =
      runtime_scene->world().entity(*returned_runtime_entity_id);
  auto *returned_runtime_transform =
      returned_runtime_entity.get<scene::Transform>();
  ASSERT_NE(returned_runtime_transform, nullptr);
  EXPECT_FLOAT_EQ(returned_runtime_transform->position.x, 1.0f);
}

TEST(SceneSerializerTest, RuntimeDoesNotTrackPreviewEditsWithoutPromotion) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);
  ASSERT_TRUE(
      SceneManager::get()->promote_source_to_preview(std::string(k_scene_id))
  );

  auto *preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  ASSERT_NE(preview_scene, nullptr);
  ASSERT_TRUE(SceneManager::get()->promote_preview(std::string(k_scene_id)));

  const auto preview_authored_entity_id =
      find_world_entity_id(preview_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(preview_authored_entity_id.has_value());
  auto preview_authored_entity =
      preview_scene->world().entity(*preview_authored_entity_id);
  auto *preview_transform =
      preview_authored_entity.get<scene::Transform>();
  ASSERT_NE(preview_transform, nullptr);
  preview_transform->position.x = 91.0f;
  preview_scene->world().touch();

  auto *runtime_scene =
      SceneManager::get()->activate_runtime(std::string(k_scene_id));
  ASSERT_NE(runtime_scene, nullptr);
  const auto runtime_authored_entity_id =
      find_world_entity_id(runtime_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(runtime_authored_entity_id.has_value());
  auto runtime_authored_entity =
      runtime_scene->world().entity(*runtime_authored_entity_id);
  auto *runtime_transform =
      runtime_authored_entity.get<scene::Transform>();
  ASSERT_NE(runtime_transform, nullptr);
  EXPECT_FLOAT_EQ(runtime_transform->position.x, 1.0f);
}

TEST(SceneSerializerTest, RuntimeEditsSurvivePreviewRefreshUntilNextPromotion) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);
  ASSERT_TRUE(
      SceneManager::get()->promote_source_to_preview(std::string(k_scene_id))
  );

  auto *preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  ASSERT_NE(preview_scene, nullptr);
  ASSERT_TRUE(SceneManager::get()->promote_preview(std::string(k_scene_id)));

  auto *runtime_scene =
      SceneManager::get()->activate_runtime(std::string(k_scene_id));
  ASSERT_NE(runtime_scene, nullptr);
  const auto runtime_authored_entity_id =
      find_world_entity_id(runtime_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(runtime_authored_entity_id.has_value());
  auto runtime_authored_entity =
      runtime_scene->world().entity(*runtime_authored_entity_id);
  auto *runtime_transform =
      runtime_authored_entity.get<scene::Transform>();
  ASSERT_NE(runtime_transform, nullptr);
  runtime_transform->position.x = 77.0f;
  runtime_scene->world().touch();

  const auto source_authored_entity_id =
      find_world_entity_id(source_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(source_authored_entity_id.has_value());
  auto source_authored_entity = source_scene->world().entity(*source_authored_entity_id);
  auto *source_transform = source_authored_entity.get<scene::Transform>();
  ASSERT_NE(source_transform, nullptr);
  source_transform->position.x = 33.0f;
  source_scene->world().touch();

  ASSERT_TRUE(
      SceneManager::get()->promote_source_to_preview(std::string(k_scene_id))
  );

  runtime_scene = SceneManager::get()->activate_runtime(std::string(k_scene_id));
  ASSERT_NE(runtime_scene, nullptr);
  const auto returned_runtime_entity_id =
      find_world_entity_id(runtime_scene->world(), k_authored_entity_name);
  ASSERT_TRUE(returned_runtime_entity_id.has_value());
  auto returned_runtime_entity =
      runtime_scene->world().entity(*returned_runtime_entity_id);
  auto *returned_runtime_transform =
      returned_runtime_entity.get<scene::Transform>();
  ASSERT_NE(returned_runtime_transform, nullptr);
  EXPECT_FLOAT_EQ(returned_runtime_transform->position.x, 77.0f);
}

TEST(SceneSerializerTest, StartupTargetCanActivatePreviewAndRuntimeScenes) {
  auto *source_scene = activate_test_scene(SceneStartupTarget::Source);
  ASSERT_NE(source_scene, nullptr);
  source_scene->set_derived_state(make_derived_state());
  source_scene->save_source();
  source_scene->build_preview();
  auto *preview_scene =
      SceneManager::get()->activate_preview(std::string(k_scene_id));
  ASSERT_NE(preview_scene, nullptr);
  ASSERT_TRUE(preview_scene->promote_preview_to_runtime());

  auto *fresh_preview_scene =
      activate_test_scene(SceneStartupTarget::Preview, false);
  ASSERT_NE(fresh_preview_scene, nullptr);
  ASSERT_EQ(fresh_preview_scene->get_session_kind(), SceneSessionKind::Preview);
  EXPECT_EQ(fresh_preview_scene->get_scene_id(), k_scene_id);
  EXPECT_TRUE(fresh_preview_scene->is_ready());

  auto *fresh_runtime_scene =
      activate_test_scene(SceneStartupTarget::Runtime, false);
  ASSERT_NE(fresh_runtime_scene, nullptr);
  ASSERT_EQ(fresh_runtime_scene->get_session_kind(), SceneSessionKind::Runtime);
  EXPECT_EQ(fresh_runtime_scene->get_scene_id(), k_scene_id);
  EXPECT_TRUE(fresh_runtime_scene->is_ready());
}

} // namespace
} // namespace astralix
