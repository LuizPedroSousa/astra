#include "axmesh-serializer.hpp"
#include "scene-serializer.hpp"

#include "arena.hpp"
#include "components/mesh.hpp"
#include "components/tags.hpp"
#include "components/transform.hpp"
#include "console.hpp"
#include "entities/scene.hpp"
#include "managers/project-manager.hpp"
#include "managers/scene-manager.hpp"
#include "project.hpp"
#include "stream-buffer.hpp"

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace astralix {
namespace {

class SceneSerializerTestScene : public Scene {
public:
  SceneSerializerTestScene() : Scene("SceneSerializerTestScene") {}

  void update() override {}

protected:
  void build_default_world() override {
    auto entity = spawn_entity("serializer-entity");
    entity.emplace<scene::SceneEntity>();
    entity.emplace<rendering::Renderable>();
    entity.emplace<rendering::ShadowCaster>();
    entity.emplace<scene::Transform>(scene::Transform{
        .position = glm::vec3(1.0f, 2.0f, 3.0f),
        .scale = glm::vec3(4.0f, 5.0f, 6.0f),
        .rotation = glm::quat(1.0f, 0.25f, 0.5f, 0.75f),
        .matrix = glm::mat4(1.0f),
        .dirty = false,
    });
    entity.emplace<rendering::MeshSet>(rendering::MeshSet{
        .meshes = {make_test_mesh()},
    });

    auto duplicate = spawn_entity("serializer-duplicate");
    duplicate.emplace<scene::SceneEntity>();
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
  }

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
};

Scope<StreamBuffer> make_buffer(const std::string &content) {
  auto buffer = create_scope<StreamBuffer>(content.size());
  if (!content.empty()) {
    buffer->write(const_cast<char *>(content.data()), content.size());
  }
  return buffer;
}

std::string context_to_string(Ref<SerializationContext> ctx) {
  ElasticArena arena(KB(4));
  auto *block = ctx->to_buffer(arena);
  return std::string(block->data, block->size);
}

Scene *activate_test_scene() {
  const auto project_directory =
      std::filesystem::temp_directory_path() / "astralix-scene-serializer-tests";
  std::filesystem::remove_all(project_directory);
  std::filesystem::create_directories(project_directory);

  ConsoleManager::get().reset_for_testing();
  ProjectManager::init();
  SceneManager::init();

  ProjectConfig config;
  config.directory = project_directory.string();
  config.serialization.format = SerializationFormat::Json;
  config.scenes.startup = "serializer";
  config.scenes.entries.push_back(ProjectSceneEntryConfig{
      .id = "serializer",
      .type = "SceneSerializerTestScene",
      .path = "scenes/serializer.axscene",
  });

  Ref<Project> project(new Project(config), [](Project *) {});
  ProjectManager::get()->add_project(project);
  SceneManager::get()->register_scene_type<SceneSerializerTestScene>(
      "SceneSerializerTestScene");

  return SceneManager::get()->activate("serializer");
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

TEST(SceneSerializerTest, WritesComponentFieldsAsNestedObjects) {
  auto *scene = activate_test_scene();
  ASSERT_NE(scene, nullptr);
  ASSERT_NE(scene->get_serializer(), nullptr);

  scene->get_serializer()->serialize();
  auto ctx = scene->get_serializer()->get_ctx();

  ASSERT_NE(ctx, nullptr);
  const int transform_index =
      find_component_index((*ctx)["entities"][0], "Transform");
  ASSERT_GE(transform_index, 0);

  auto fields_ctx =
      (*ctx)["entities"][0]["components"][transform_index]["fields"];
  EXPECT_EQ(fields_ctx.kind(), SerializationTypeKind::Object);
  EXPECT_EQ(fields_ctx["position"].kind(), SerializationTypeKind::Object);
  EXPECT_EQ(fields_ctx["scale"].kind(), SerializationTypeKind::Object);
  EXPECT_EQ(fields_ctx["rotation"].kind(), SerializationTypeKind::Object);
  EXPECT_FLOAT_EQ(fields_ctx["position"]["x"].as<float>(), 1.0f);
  EXPECT_FLOAT_EQ(fields_ctx["position"]["y"].as<float>(), 2.0f);
  EXPECT_FLOAT_EQ(fields_ctx["position"]["z"].as<float>(), 3.0f);
  EXPECT_FLOAT_EQ(fields_ctx["rotation"]["w"].as<float>(), 1.0f);
  EXPECT_FALSE(fields_ctx["dirty"].as<bool>());

  const auto field_keys = fields_ctx.object_keys();
  EXPECT_EQ(field_keys.size(), 4u);
  EXPECT_NE(std::find(field_keys.begin(), field_keys.end(), "position"),
            field_keys.end());
  EXPECT_NE(std::find(field_keys.begin(), field_keys.end(), "scale"),
            field_keys.end());
  EXPECT_NE(std::find(field_keys.begin(), field_keys.end(), "rotation"),
            field_keys.end());
  EXPECT_NE(std::find(field_keys.begin(), field_keys.end(), "dirty"),
            field_keys.end());
}

TEST(SceneSerializerTest, WritesZeroFieldComponentsAsEntityTags) {
  auto *scene = activate_test_scene();
  ASSERT_NE(scene, nullptr);
  ASSERT_NE(scene->get_serializer(), nullptr);

  scene->get_serializer()->serialize();
  auto ctx = scene->get_serializer()->get_ctx();
  ASSERT_NE(ctx, nullptr);

  auto tags_ctx = (*ctx)["entities"][0]["tags"];
  ASSERT_EQ(tags_ctx.kind(), SerializationTypeKind::Array);
  EXPECT_TRUE(contains_string(tags_ctx, "SceneEntity"));
  EXPECT_TRUE(contains_string(tags_ctx, "Renderable"));
  EXPECT_TRUE(contains_string(tags_ctx, "ShadowCaster"));

  EXPECT_EQ(find_component_index((*ctx)["entities"][0], "SceneEntity"), -1);
  EXPECT_EQ(find_component_index((*ctx)["entities"][0], "Renderable"), -1);
  EXPECT_EQ(find_component_index((*ctx)["entities"][0], "ShadowCaster"), -1);
}

TEST(SceneSerializerTest, RoundTripsNestedFieldObjects) {
  auto *scene = activate_test_scene();
  ASSERT_NE(scene, nullptr);
  ASSERT_NE(scene->get_serializer(), nullptr);

  scene->get_serializer()->serialize();
  auto ctx = scene->get_serializer()->get_ctx();
  ASSERT_NE(ctx, nullptr);

  const std::string nested_json = context_to_string(ctx);
  EXPECT_EQ((*ctx)["scene"]["version"].as<int>(), 3);
  EXPECT_EQ(nested_json.find("\"name\":\"position.x\""), std::string::npos);
  EXPECT_EQ(nested_json.find("\"type\":\"Renderable\""), std::string::npos);

  scene->world() = ecs::World();
  ctx->from_buffer(make_buffer(nested_json));
  scene->get_serializer()->deserialize();

  const auto entity_id =
      EntityID(std::stoull((*ctx)["entities"][0]["id"].as<std::string>()));
  auto entity = scene->world().entity(entity_id);
  ASSERT_TRUE(entity.exists());

  auto *transform = entity.get<scene::Transform>();
  ASSERT_NE(transform, nullptr);
  EXPECT_EQ(entity.name(), std::string_view("serializer-entity"));
  EXPECT_EQ(transform->position, glm::vec3(1.0f, 2.0f, 3.0f));
  EXPECT_EQ(transform->scale, glm::vec3(4.0f, 5.0f, 6.0f));
  EXPECT_FLOAT_EQ(transform->rotation.w, 1.0f);
  EXPECT_FLOAT_EQ(transform->rotation.x, 0.25f);
  EXPECT_FLOAT_EQ(transform->rotation.y, 0.5f);
  EXPECT_FLOAT_EQ(transform->rotation.z, 0.75f);
  EXPECT_TRUE(transform->dirty);
  EXPECT_NE(entity.get<scene::SceneEntity>(), nullptr);
  EXPECT_NE(entity.get<rendering::Renderable>(), nullptr);
  EXPECT_NE(entity.get<rendering::ShadowCaster>(), nullptr);
}

TEST(SceneSerializerTest, ExternalizesMeshSetsIntoPerSceneAxmeshFiles) {
  auto *scene = activate_test_scene();
  ASSERT_NE(scene, nullptr);
  ASSERT_NE(scene->get_serializer(), nullptr);

  scene->get_serializer()->serialize();
  auto ctx = scene->get_serializer()->get_ctx();
  ASSERT_NE(ctx, nullptr);

  const auto scene_directory =
      std::filesystem::temp_directory_path() / "astralix-scene-serializer-tests";
  const auto mesh_directory = scene_directory / "scenes" / "meshes";
  ASSERT_TRUE(std::filesystem::exists(mesh_directory));

  const int entity_index =
      find_entity_index((*ctx)["entities"], "serializer-entity");
  const int duplicate_index =
      find_entity_index((*ctx)["entities"], "serializer-duplicate");
  ASSERT_GE(entity_index, 0);
  ASSERT_GE(duplicate_index, 0);

  const int mesh_set_index =
      find_component_index((*ctx)["entities"][entity_index], "MeshSet");
  const int duplicate_mesh_set_index =
      find_component_index((*ctx)["entities"][duplicate_index], "MeshSet");
  ASSERT_GE(mesh_set_index, 0);
  ASSERT_GE(duplicate_mesh_set_index, 0);

  auto fields_ctx =
      (*ctx)["entities"][entity_index]["components"][mesh_set_index]["fields"];
  auto duplicate_fields_ctx = (*ctx)["entities"][duplicate_index]["components"]
                                   [duplicate_mesh_set_index]["fields"];

  EXPECT_EQ(fields_ctx.kind(), SerializationTypeKind::Object);
  EXPECT_EQ(fields_ctx["asset"].kind(), SerializationTypeKind::Object);
  EXPECT_EQ(fields_ctx["asset"]["path"].kind(), SerializationTypeKind::String);
  EXPECT_EQ(fields_ctx["mesh_count"].kind(), SerializationTypeKind::Unknown);
  EXPECT_EQ(fields_ctx["asset"]["path"].as<std::string>(),
            duplicate_fields_ctx["asset"]["path"].as<std::string>());

  const auto asset_path =
      scene_directory / fields_ctx["asset"]["path"].as<std::string>();
  ASSERT_TRUE(std::filesystem::exists(asset_path));

  size_t mesh_file_count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(mesh_directory)) {
    if (entry.path().extension() == ".axmesh") {
      ++mesh_file_count;
    }
  }
  EXPECT_EQ(mesh_file_count, 1u);

  const auto meshes = AxMeshSerializer::read(asset_path);
  ASSERT_EQ(meshes.size(), 1u);
  EXPECT_EQ(meshes[0].vertices.size(), 3u);
  EXPECT_EQ(meshes[0].indices.size(), 3u);

  scene->world() = ecs::World();
  scene->get_serializer()->deserialize();

  auto entity = scene->world().entity(
      EntityID(
          std::stoull((*ctx)["entities"][entity_index]["id"].as<std::string>())
      )
  );
  ASSERT_TRUE(entity.exists());

  auto *mesh_set = entity.get<rendering::MeshSet>();
  ASSERT_NE(mesh_set, nullptr);
  ASSERT_EQ(mesh_set->meshes.size(), 1u);
  EXPECT_EQ(mesh_set->meshes[0].vertices.size(), 3u);
  EXPECT_EQ(mesh_set->meshes[0].indices.size(), 3u);
}

} // namespace
} // namespace astralix
