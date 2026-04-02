#include "scene-serializer.hpp"
#include "assert.hpp"
#include "axmesh-serializer.hpp"
#include "components/serialization/mesh.hpp"
#include "context-proxy.hpp"
#include "entities/scene.hpp"
#include "log.hpp"
#include "managers/project-manager.hpp"
#include "scene-snapshot.hpp"
#include "string"
#include <bit>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <vector>

namespace astralix {
namespace {

constexpr int k_scene_version = 1;

SerializationFormat scene_serialization_format() {
  auto project = active_project();
  return project != nullptr ? project->get_config().serialization.format
                            : SerializationFormat::Json;
}

void reset_context(Ref<SerializationContext> &ctx) {
  ctx = SerializationContext::create(scene_serialization_format());
}

std::filesystem::path scene_absolute_path(const Scene &scene) {
  auto project = active_project();
  ASTRA_ENSURE(project == nullptr, "Cannot resolve scene path without an active project");
  ASTRA_ENSURE(scene.get_scene_path().empty(), "Scene path is not configured for scene ", scene.get_type());

  return project->resolve_path(scene.get_scene_path());
}

std::filesystem::path generated_mesh_directory(const Scene &scene) {
  return scene_absolute_path(scene).parent_path() / "meshes";
}

std::string project_relative_path(const std::filesystem::path &path) {
  auto project = active_project();
  ASTRA_ENSURE(project == nullptr, "Cannot resolve project-relative path without an active project");

  return path.lexically_relative(
                 std::filesystem::path(project->get_config().directory)
  )
      .generic_string();
}

void fnv1a_update(uint64_t &hash, const void *data, size_t size) {
  const auto *bytes = static_cast<const uint8_t *>(data);
  for (size_t index = 0; index < size; ++index) {
    hash ^= static_cast<uint64_t>(bytes[index]);
    hash *= 1099511628211ull;
  }
}

template <typename T>
void fnv1a_update_value(uint64_t &hash, const T &value) {
  fnv1a_update(hash, &value, sizeof(value));
}

void fnv1a_update_float(uint64_t &hash, float value) {
  const uint32_t bits = std::bit_cast<uint32_t>(value);
  fnv1a_update_value(hash, bits);
}

std::string hash_mesh_set(const std::vector<Mesh> &meshes) {
  uint64_t hash = 1469598103934665603ull;
  fnv1a_update_value(hash, static_cast<uint64_t>(meshes.size()));

  for (const auto &mesh : meshes) {
    fnv1a_update_value(hash, static_cast<int>(mesh.draw_type));
    fnv1a_update_value(hash, static_cast<uint64_t>(mesh.vertices.size()));
    fnv1a_update_value(hash, static_cast<uint64_t>(mesh.indices.size()));

    for (const auto &vertex : mesh.vertices) {
      fnv1a_update_float(hash, vertex.position.x);
      fnv1a_update_float(hash, vertex.position.y);
      fnv1a_update_float(hash, vertex.position.z);
      fnv1a_update_float(hash, vertex.normal.x);
      fnv1a_update_float(hash, vertex.normal.y);
      fnv1a_update_float(hash, vertex.normal.z);
      fnv1a_update_float(hash, vertex.texture_coordinates.x);
      fnv1a_update_float(hash, vertex.texture_coordinates.y);
      fnv1a_update_float(hash, vertex.tangent.x);
      fnv1a_update_float(hash, vertex.tangent.y);
      fnv1a_update_float(hash, vertex.tangent.z);
    }

    for (unsigned int index : mesh.indices) {
      fnv1a_update_value(hash, index);
    }
  }

  std::ostringstream stream;
  stream << std::hex << std::setfill('0') << std::setw(16) << hash;
  return stream.str();
}

serialization::ComponentSnapshot
externalize_component_snapshot(const Scene &scene, const serialization::ComponentSnapshot &component) {
  if (component.name != "MeshSet") {
    return component;
  }

  const auto meshes = serialization::read_mesh_set(component.fields);
  const std::string asset_hash = hash_mesh_set(meshes);
  const auto asset_path =
      generated_mesh_directory(scene) / (asset_hash + ".axmesh");

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

SerializedEntityData split_entity_snapshot(
    const Scene &scene, const serialization::EntitySnapshot &entity
) {
  SerializedEntityData serialized;
  serialized.tags.reserve(entity.components.size());
  serialized.components.reserve(entity.components.size());

  for (const auto &snapshot : entity.components) {
    const auto component = externalize_component_snapshot(scene, snapshot);
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

  const auto meshes =
      AxMeshSerializer::read(project->resolve_path(*asset_path));
  component.fields =
      serialization::snapshot_component(rendering::MeshSet{.meshes = meshes})
          .fields;
}

void write_nested_field(ContextProxy ctx, std::string_view path, const SerializableValue &value) {
  const size_t separator = path.find('.');
  if (separator == std::string_view::npos) {
    ctx[std::string(path)] = value;
    return;
  }

  write_nested_field(ctx[std::string(path.substr(0, separator))], path.substr(separator + 1), value);
}

std::optional<SerializableValue> read_serializable_value(ContextProxy &ctx) {
  switch (ctx.kind()) {
    case SerializationTypeKind::String:
      return SerializableValue(ctx.as<std::string>());
    case SerializationTypeKind::Int:
      return SerializableValue(ctx.as<int>());
    case SerializationTypeKind::Float:
      return SerializableValue(ctx.as<float>());
    case SerializationTypeKind::Bool:
      return SerializableValue(ctx.as<bool>());
    default:
      return std::nullopt;
  }
}

void append_nested_fields(ContextProxy &field_ctx, std::string_view prefix, serialization::fields::FieldList &fields) {
  if (auto value = read_serializable_value(field_ctx); value.has_value()) {
    if (!prefix.empty()) {
      fields.push_back(serialization::fields::Field{
          .name = std::string(prefix),
          .value = std::move(*value),
      });
    }
    return;
  }

  if (field_ctx.kind() != SerializationTypeKind::Object) {
    return;
  }

  for (const auto &key : field_ctx.object_keys()) {
    const std::string field_name =
        prefix.empty() ? key : std::string(prefix) + "." + key;
    auto child_ctx = field_ctx[key];
    append_nested_fields(child_ctx, field_name, fields);
  }
}

std::optional<serialization::ComponentSnapshot>
read_component_snapshot(ContextProxy component_ctx) {
  if (component_ctx["type"].kind() != SerializationTypeKind::String) {
    return std::nullopt;
  }

  serialization::ComponentSnapshot component{
      .name = component_ctx["type"].as<std::string>(),
  };

  auto fields_ctx = component_ctx["fields"];
  component.fields.reserve(fields_ctx.size());

  ASTRA_ENSURE(fields_ctx.kind() == SerializationTypeKind::Array, "Legacy array-based scene fields are no longer supported for "
                                                                  "component ",
               component.name);
  append_nested_fields(fields_ctx, "", component.fields);

  return component;
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

} // namespace

SceneSerializer::SceneSerializer(Ref<Scene> scene) { m_scene = scene; }

SceneSerializer::SceneSerializer() {}

void SceneSerializer::serialize() {
  if (m_scene == nullptr) {
    return;
  }

  reset_context(m_ctx);
  SerializationContext &ctx = *m_ctx.get();

  ASTRA_ENSURE(m_scene->get_scene_id().empty(), "Scene id is not configured for scene type ", m_scene->get_type());

  ctx["scene"]["version"] = k_scene_version;
  ctx["scene"]["id"] = m_scene->get_scene_id();
  ctx["scene"]["type"] = m_scene->get_type();

  auto entities = serialization::collect_scene_snapshots(m_scene->world());
  std::vector<SerializedEntityData> serialized_entities;
  serialized_entities.reserve(entities.size());

  for (const auto &entity : entities) {
    serialized_entities.push_back(split_entity_snapshot(*m_scene, entity));
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

      for (size_t k = 0; k < component.fields.size(); ++k) {
        const auto &field = component.fields[k];
        write_nested_field(component_ctx["fields"], field.name, field.value);
      }
    }
  }
}

void SceneSerializer::deserialize() {
  if (m_scene == nullptr || m_ctx == nullptr) {
    return;
  }

  std::vector<serialization::EntitySnapshot> entities;

  ASTRA_ENSURE((*m_ctx)["scene"]["version"].kind() != SerializationTypeKind::Int, "Scene version is missing or invalid");
  ASTRA_ENSURE((*m_ctx)["scene"]["version"].as<int>() != k_scene_version, "Unsupported scene version: ", (*m_ctx)["scene"]["version"].as<int>());
  ASTRA_ENSURE((*m_ctx)["scene"]["id"].kind() != SerializationTypeKind::String, "Scene id is missing");
  ASTRA_ENSURE((*m_ctx)["scene"]["type"].kind() != SerializationTypeKind::String, "Scene type is missing");

  const std::string scene_id = (*m_ctx)["scene"]["id"].as<std::string>();
  const std::string scene_type = (*m_ctx)["scene"]["type"].as<std::string>();

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
}
} // namespace astralix
