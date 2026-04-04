#include "scene-serializer.hpp"

#include "assert.hpp"
#include "axmesh-serializer.hpp"
#include "components/serialization/mesh.hpp"
#include "context-proxy.hpp"
#include "entities/scene.hpp"
#include "entities/serializers/component-snapshot-context.hpp"
#include "entities/serializers/derived-override-serializer.hpp"
#include "fnv1a.hpp"
#include "log.hpp"
#include "managers/project-manager.hpp"
#include "scene-snapshot.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

namespace astralix {
namespace {

constexpr int k_scene_version = 1;

std::string scene_artifact_kind_to_string(SceneArtifactKind kind) {
  switch (kind) {
    case SceneArtifactKind::Source:
      return "source";
    case SceneArtifactKind::Preview:
      return "preview";
    case SceneArtifactKind::Runtime:
      return "runtime";
    default:
      ASTRA_EXCEPTION("Unknown scene artifact kind");
  }
}

SceneArtifactKind scene_artifact_kind_from_string(const std::string &kind) {
  if (kind == "source") {
    return SceneArtifactKind::Source;
  }

  if (kind == "preview") {
    return SceneArtifactKind::Preview;
  }

  if (kind == "runtime") {
    return SceneArtifactKind::Runtime;
  }

  ASTRA_EXCEPTION("Unknown scene artifact kind: ", kind);
}

SerializationFormat scene_serialization_format() {
  auto project = active_project();
  return project != nullptr ? project->get_config().serialization.format
                            : SerializationFormat::Json;
}

void reset_context(Ref<SerializationContext> &ctx) {
  ctx = SerializationContext::create(scene_serialization_format());
}

std::filesystem::path scene_absolute_path(
    const Scene &scene, SceneArtifactKind artifact_kind
) {
  auto project = active_project();
  ASTRA_ENSURE(project == nullptr, "Cannot resolve scene path without an active project");

  switch (artifact_kind) {
    case SceneArtifactKind::Source:
      return project->resolve_path(scene.get_source_scene_path());
    case SceneArtifactKind::Preview:
      return project->resolve_path(scene.get_preview_scene_path());
    case SceneArtifactKind::Runtime:
      return project->resolve_path(scene.get_runtime_scene_path());
  }

  ASTRA_EXCEPTION("Unknown scene artifact kind");
}

std::filesystem::path generated_mesh_directory(
    const Scene &scene, SceneArtifactKind artifact_kind
) {
  const auto parent = scene_absolute_path(scene, artifact_kind).parent_path();
  return artifact_kind == SceneArtifactKind::Preview ? parent / "preview-meshes"
                                                     : parent / "meshes";
}

std::string project_relative_path(const std::filesystem::path &path) {
  auto project = active_project();
  ASTRA_ENSURE(project == nullptr, "Cannot resolve project-relative path without an active project");

  return path.lexically_relative(
                 std::filesystem::path(project->get_config().directory)
  )
      .generic_string();
}

std::string hash_mesh_set(const std::vector<Mesh> &meshes) {
  constexpr uint64_t k_mesh_set_hash_offset_basis = 1469598103934665603ull;

  uint64_t hash = k_mesh_set_hash_offset_basis;
  hash = fnv1a64_append_value(hash, static_cast<uint64_t>(meshes.size()));

  for (const auto &mesh : meshes) {
    hash = fnv1a64_append_value(hash, static_cast<int>(mesh.draw_type));
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(mesh.vertices.size()));
    hash = fnv1a64_append_value(hash, static_cast<uint64_t>(mesh.indices.size()));

    for (const auto &vertex : mesh.vertices) {
      hash = fnv1a64_append_value(
          hash, std::bit_cast<uint32_t>(vertex.position.x)
      );
      hash = fnv1a64_append_value(
          hash, std::bit_cast<uint32_t>(vertex.position.y)
      );
      hash = fnv1a64_append_value(
          hash, std::bit_cast<uint32_t>(vertex.position.z)
      );
      hash = fnv1a64_append_value(hash, std::bit_cast<uint32_t>(vertex.normal.x));
      hash = fnv1a64_append_value(hash, std::bit_cast<uint32_t>(vertex.normal.y));
      hash = fnv1a64_append_value(hash, std::bit_cast<uint32_t>(vertex.normal.z));
      hash = fnv1a64_append_value(
          hash, std::bit_cast<uint32_t>(vertex.texture_coordinates.x)
      );
      hash = fnv1a64_append_value(
          hash, std::bit_cast<uint32_t>(vertex.texture_coordinates.y)
      );
      hash = fnv1a64_append_value(hash, std::bit_cast<uint32_t>(vertex.tangent.x));
      hash = fnv1a64_append_value(hash, std::bit_cast<uint32_t>(vertex.tangent.y));
      hash = fnv1a64_append_value(hash, std::bit_cast<uint32_t>(vertex.tangent.z));
    }

    for (unsigned int index : mesh.indices) {
      hash = fnv1a64_append_value(hash, index);
    }
  }

  return fnv1a64_hex_digest(hash);
}

serialization::ComponentSnapshot externalize_component_snapshot(
    const Scene &scene,
    const serialization::ComponentSnapshot &component,
    SceneArtifactKind artifact_kind
) {
  if (component.name != "MeshSet") {
    return component;
  }

  const auto meshes = serialization::read_mesh_set(component.fields);
  const std::string asset_hash = hash_mesh_set(meshes);
  const auto asset_path =
      generated_mesh_directory(scene, artifact_kind) / (asset_hash + ".axmesh");

  if (!std::filesystem::exists(asset_path)) {
    AxMeshSerializer::write(asset_path, meshes);
  }

  return serialization::ComponentSnapshot{
      .name = component.name,
      .fields = {
          serialization::fields::Field{
              .name = "asset.path",
              .value = project_relative_path(asset_path),
          },
      },
  };
}

bool is_tag_component(const serialization::ComponentSnapshot &component) {
  return component.fields.empty();
}

struct SerializedEntityData {
  std::vector<std::string> tags;
  std::vector<serialization::ComponentSnapshot> components;
};

bool has_component_named(const serialization::EntitySnapshot &entity, std::string_view name) {
  return std::any_of(
      entity.components.begin(),
      entity.components.end(),
      [name](const auto &component) { return component.name == name; }
  );
}

bool should_include_entity_in_artifact(
    const serialization::EntitySnapshot &entity, SceneArtifactKind artifact_kind
) {
  switch (artifact_kind) {
    case SceneArtifactKind::Source:
      return !has_component_named(entity, "DerivedEntity");
    case SceneArtifactKind::Preview:
    case SceneArtifactKind::Runtime:
      return !has_component_named(entity, "EditorOnly") &&
             !has_component_named(entity, "GeneratorSpec");
  }

  return true;
}

bool should_strip_component_for_artifact(
    const serialization::ComponentSnapshot &component,
    SceneArtifactKind artifact_kind
) {
  return artifact_kind == SceneArtifactKind::Runtime &&
         (component.name == "MetaEntityOwner" ||
          component.name == "DerivedEntity");
}

std::vector<serialization::EntitySnapshot> filter_entities_for_artifact(
    std::vector<serialization::EntitySnapshot> entities,
    SceneArtifactKind artifact_kind
) {
  entities.erase(
      std::remove_if(
          entities.begin(),
          entities.end(),
          [&](const auto &entity) {
            return !should_include_entity_in_artifact(entity, artifact_kind);
          }
      ),
      entities.end()
  );

  for (auto &entity : entities) {
    entity.components.erase(
        std::remove_if(
            entity.components.begin(),
            entity.components.end(),
            [&](const auto &component) {
              return should_strip_component_for_artifact(
                  component, artifact_kind
              );
            }
        ),
        entity.components.end()
    );
  }

  return entities;
}

SerializedEntityData split_entity_snapshot(
    const Scene &scene,
    const serialization::EntitySnapshot &entity,
    SceneArtifactKind artifact_kind
) {
  SerializedEntityData serialized;
  serialized.tags.reserve(entity.components.size());
  serialized.components.reserve(entity.components.size());

  for (const auto &snapshot : entity.components) {
    const auto component =
        externalize_component_snapshot(scene, snapshot, artifact_kind);
    if (is_tag_component(component)) {
      serialized.tags.push_back(component.name);
    } else {
      serialized.components.push_back(component);
    }
  }

  return serialized;
}

void inline_external_mesh_set(serialization::ComponentSnapshot &component) {
  if (component.name != "MeshSet") {
    return;
  }

  auto asset_path =
      serialization::fields::read_string(component.fields, "asset.path");
  ASTRA_ENSURE(!asset_path.has_value() || asset_path->empty(), "MeshSet asset path is required");

  auto project = active_project();
  ASTRA_ENSURE(project == nullptr, "Cannot resolve MeshSet asset without an active project");

  const auto meshes = AxMeshSerializer::read(project->resolve_path(*asset_path));
  component.fields =
      serialization::snapshot_component(rendering::MeshSet{.meshes = meshes})
          .fields;
}

std::optional<serialization::EntitySnapshot>
read_entity_snapshot(ContextProxy entity_ctx) {
  if (entity_ctx["id"].kind() != SerializationTypeKind::String ||
      entity_ctx["name"].kind() != SerializationTypeKind::String) {
    return std::nullopt;
  }

  serialization::EntitySnapshot entity{
      .id = EntityID(std::stoull(entity_ctx["id"].as<std::string>())),
      .name = entity_ctx["name"].as<std::string>(),
      .active = entity_ctx["active"].kind() == SerializationTypeKind::Bool
                    ? entity_ctx["active"].as<bool>()
                    : true,
  };

  auto tags_ctx = entity_ctx["tags"];
  if (tags_ctx.kind() == SerializationTypeKind::Array) {
    entity.components.reserve(entity_ctx["components"].size() + tags_ctx.size());

    for (size_t tag_index = 0; tag_index < tags_ctx.size(); ++tag_index) {
      auto tag_ctx = tags_ctx[static_cast<int>(tag_index)];
      ASTRA_ENSURE(tag_ctx.kind() != SerializationTypeKind::String, "Entity tag must be a string");
      entity.components.push_back(serialization::ComponentSnapshot{
          .name = tag_ctx.as<std::string>(),
      });
    }
  }

  const size_t component_count = entity_ctx["components"].size();
  entity.components.reserve(entity.components.size() + component_count);

  for (size_t component_index = 0; component_index < component_count;
       ++component_index) {
    auto component = read_component_snapshot(
        entity_ctx["components"][static_cast<int>(component_index)]
    );
    if (component.has_value()) {
      inline_external_mesh_set(*component);
      entity.components.push_back(std::move(*component));
    }
  }

  return entity;
}

void serialize_entities_to_context(
    SceneSerializer &serializer,
    const Scene &scene,
    const std::vector<serialization::EntitySnapshot> &entities
) {
  SerializationContext &ctx = *serializer.get_ctx().get();

  ctx["scene"]["version"] = k_scene_version;
  ctx["scene"]["kind"] =
      scene_artifact_kind_to_string(serializer.get_artifact_kind());
  ctx["scene"]["id"] = scene.get_scene_id();
  ctx["scene"]["type"] = scene.get_type();

  if (serializer.get_artifact_kind() == SceneArtifactKind::Source) {
    serialize_derived_state(ctx, scene.get_derived_state());
  } else if (serializer.get_artifact_kind() == SceneArtifactKind::Preview) {
    if (const auto &preview_info = scene.get_preview_build_info();
        preview_info.has_value()) {
      ctx["build"]["source_revision"] =
          static_cast<int>(preview_info->source_revision);
      ctx["build"]["built_at_utc"] = preview_info->built_at_utc;
    }
  } else if (serializer.get_artifact_kind() == SceneArtifactKind::Runtime) {
    if (const auto &runtime_info = scene.get_runtime_promotion_info();
        runtime_info.has_value()) {
      ctx["build"]["promoted_from_preview_revision"] =
          static_cast<int>(runtime_info->promoted_from_preview_revision);
      ctx["build"]["promoted_at_utc"] = runtime_info->promoted_at_utc;
    }
  }

  std::vector<SerializedEntityData> serialized_entities;
  serialized_entities.reserve(entities.size());

  for (const auto &entity : entities) {
    serialized_entities.push_back(
        split_entity_snapshot(scene, entity, serializer.get_artifact_kind())
    );
  }

  for (size_t i = 0; i < entities.size(); ++i) {
    const auto &entity = entities[i];
    const auto &serialized = serialized_entities[i];

    ctx["entities"][static_cast<int>(i)]["active"] = entity.active;
    ctx["entities"][static_cast<int>(i)]["id"] =
        static_cast<std::string>(entity.id);
    ctx["entities"][static_cast<int>(i)]["name"] = entity.name;

    for (size_t j = 0; j < serialized.tags.size(); ++j) {
      ctx["entities"][static_cast<int>(i)]["tags"][static_cast<int>(j)] =
          serialized.tags[j];
    }

    for (size_t j = 0; j < serialized.components.size(); ++j) {
      const auto &component = serialized.components[j];
      auto component_ctx =
          ctx["entities"][static_cast<int>(i)]["components"][static_cast<int>(j)];
      component_ctx["type"] = component.name;

      for (const auto &field : component.fields) {
        write_nested_field(component_ctx["fields"], field.name, field.value);
      }
    }
  }
}

} // namespace

SceneSerializer::SceneSerializer(Ref<Scene> scene)
    : Serializer(), m_scene(scene), m_artifact_kind(SceneArtifactKind::Source) {}

SceneSerializer::SceneSerializer()
    : Serializer(), m_artifact_kind(SceneArtifactKind::Source) {}

void SceneSerializer::serialize() {
  if (m_scene == nullptr) {
    return;
  }

  serialize_snapshots(
      filter_entities_for_artifact(
          serialization::collect_scene_snapshots(m_scene->world()),
          m_artifact_kind
      )
  );
}

void SceneSerializer::serialize_snapshots(
    const std::vector<serialization::EntitySnapshot> &entities
) {
  if (m_scene == nullptr) {
    return;
  }

  ASTRA_ENSURE(m_scene->get_scene_id().empty(), "Scene id is not configured for scene type ", m_scene->get_type());
  reset_context(m_ctx);
  serialize_entities_to_context(*this, *m_scene, entities);
}

void SceneSerializer::deserialize() {
  if (m_scene == nullptr || m_ctx == nullptr) {
    return;
  }

  std::vector<serialization::EntitySnapshot> entities;

  ASTRA_ENSURE(
      (*m_ctx)["scene"]["version"].kind() != SerializationTypeKind::Int,
      "Scene version is missing or invalid"
  );
  const int scene_version = (*m_ctx)["scene"]["version"].as<int>();
  ASTRA_ENSURE(
      scene_version != k_scene_version,
      "Unsupported scene version: ",
      scene_version
  );
  ASTRA_ENSURE(
      (*m_ctx)["scene"]["kind"].kind() != SerializationTypeKind::String,
      "Scene kind is missing"
  );
  ASTRA_ENSURE(
      (*m_ctx)["scene"]["id"].kind() != SerializationTypeKind::String,
      "Scene id is missing"
  );
  ASTRA_ENSURE(
      (*m_ctx)["scene"]["type"].kind() != SerializationTypeKind::String,
      "Scene type is missing"
  );

  const auto scene_kind = scene_artifact_kind_from_string(
      (*m_ctx)["scene"]["kind"].as<std::string>()
  );
  const std::string scene_id = (*m_ctx)["scene"]["id"].as<std::string>();
  const std::string scene_type = (*m_ctx)["scene"]["type"].as<std::string>();

  ASTRA_ENSURE(scene_kind != m_artifact_kind, "Scene kind mismatch. Expected ", scene_artifact_kind_to_string(m_artifact_kind), ", got ", scene_artifact_kind_to_string(scene_kind));
  ASTRA_ENSURE(scene_id != m_scene->get_scene_id(), "Scene id mismatch. Expected ", m_scene->get_scene_id(), ", got ", scene_id);
  ASTRA_ENSURE(scene_type != m_scene->get_type(), "Scene type mismatch. Expected ", m_scene->get_type(), ", got ", scene_type);

  const size_t entity_count = (*m_ctx)["entities"].size();
  entities.reserve(entity_count);

  for (size_t index = 0; index < entity_count; ++index) {
    auto entity =
        read_entity_snapshot((*m_ctx)["entities"][static_cast<int>(index)]);
    if (entity.has_value()) {
      entities.push_back(std::move(*entity));
    }
  }

  serialization::apply_scene_snapshots(m_scene->world(), entities);

  switch (m_artifact_kind) {
    case SceneArtifactKind::Source:
      m_scene->set_derived_state(deserialize_derived_state(m_ctx));
      m_scene->set_preview_build_info(std::nullopt);
      m_scene->set_runtime_promotion_info(std::nullopt);
      break;
    case SceneArtifactKind::Preview: {
      ScenePreviewBuildInfo preview_info;
      bool has_preview_info = false;
      if ((*m_ctx)["build"]["source_revision"].kind() ==
          SerializationTypeKind::Int) {
        preview_info.source_revision = static_cast<uint64_t>(
            (*m_ctx)["build"]["source_revision"].as<int>()
        );
        has_preview_info = true;
      }
      if ((*m_ctx)["build"]["built_at_utc"].kind() ==
          SerializationTypeKind::String) {
        preview_info.built_at_utc =
            (*m_ctx)["build"]["built_at_utc"].as<std::string>();
        has_preview_info = true;
      }
      m_scene->set_preview_build_info(
          has_preview_info ? std::optional<ScenePreviewBuildInfo>(preview_info)
                           : std::nullopt
      );
      m_scene->set_runtime_promotion_info(std::nullopt);
      m_scene->set_derived_state({});
      break;
    }
    case SceneArtifactKind::Runtime: {
      SceneRuntimePromotionInfo runtime_info;
      bool has_runtime_info = false;
      if ((*m_ctx)["build"]["promoted_from_preview_revision"].kind() ==
          SerializationTypeKind::Int) {
        runtime_info.promoted_from_preview_revision = static_cast<uint64_t>(
            (*m_ctx)["build"]["promoted_from_preview_revision"].as<int>()
        );
        has_runtime_info = true;
      }
      if ((*m_ctx)["build"]["promoted_at_utc"].kind() ==
          SerializationTypeKind::String) {
        runtime_info.promoted_at_utc =
            (*m_ctx)["build"]["promoted_at_utc"].as<std::string>();
        has_runtime_info = true;
      }
      m_scene->set_runtime_promotion_info(
          has_runtime_info
              ? std::optional<SceneRuntimePromotionInfo>(runtime_info)
              : std::nullopt
      );
      m_scene->set_preview_build_info(std::nullopt);
      m_scene->set_derived_state({});
      break;
    }
  }
}

} // namespace astralix
