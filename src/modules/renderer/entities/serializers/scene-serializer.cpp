#include "context-proxy.hpp"
#include "entities/scene.hpp"
#include "log.hpp"
#include "managers/project-manager.hpp"
#include "scene-serializer.hpp"
#include "scene-snapshot.hpp"
#include "string"

namespace astralix {
namespace {

SerializationFormat scene_serialization_format() {
  auto project = active_project();
  return project != nullptr ? project->get_config().serialization.format
                            : SerializationFormat::Json;
}

void reset_context(Ref<SerializationContext> &ctx) {
  ctx = SerializationContext::create(scene_serialization_format());
}

std::optional<SerializableValue> read_serializable_value(ContextProxy ctx) {
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

std::optional<serialization::fields::Field> read_field(ContextProxy field_ctx) {
  if (field_ctx["name"].kind() != SerializationTypeKind::String) {
    return std::nullopt;
  }

  auto value = read_serializable_value(field_ctx["value"]);
  if (!value.has_value()) {
    return std::nullopt;
  }

  return serialization::fields::Field{
      .name = field_ctx["name"].as<std::string>(),
      .value = *value,
  };
}

std::optional<serialization::ComponentSnapshot>
read_component_snapshot(ContextProxy component_ctx) {
  if (component_ctx["type"].kind() != SerializationTypeKind::String) {
    return std::nullopt;
  }

  serialization::ComponentSnapshot component{
      .name = component_ctx["type"].as<std::string>(),
  };

  const size_t field_count = component_ctx["fields"].size();
  component.fields.reserve(field_count);

  for (size_t field_index = 0; field_index < field_count; ++field_index) {
    auto field = read_field(component_ctx["fields"][static_cast<int>(field_index)]);
    if (field.has_value()) {
      component.fields.push_back(std::move(*field));
    }
  }

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

  const size_t component_count = entity_ctx["components"].size();
  entity.components.reserve(component_count);

  for (size_t component_index = 0; component_index < component_count;
       ++component_index) {
    auto component = read_component_snapshot(
        entity_ctx["components"][static_cast<int>(component_index)]);
    if (component.has_value()) {
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

  auto entities = serialization::collect_scene_snapshots(m_scene->world());

  for (size_t i = 0; i < entities.size(); ++i) {
    const auto &entity = entities[i];

    ctx["entities"][static_cast<int>(i)]["active"] = entity.active;
    ctx["entities"][static_cast<int>(i)]["id"] =
        static_cast<std::string>(entity.id);
    ctx["entities"][static_cast<int>(i)]["name"] = entity.name;

    for (size_t j = 0; j < entity.components.size(); ++j) {
      const auto &component = entity.components[j];
      auto component_ctx =
          ctx["entities"][static_cast<int>(i)]["components"][static_cast<int>(j)];
      component_ctx["type"] = component.name;

      for (size_t k = 0; k < component.fields.size(); ++k) {
        const auto &field = component.fields[k];
        auto field_ctx =
            component_ctx["fields"][static_cast<int>(k)];
        field_ctx["name"] = field.name;
        field_ctx["value"] = field.value;
      }
    }
  }
}

void SceneSerializer::deserialize() {
  if (m_scene == nullptr || m_ctx == nullptr) {
    return;
  }

  std::vector<serialization::EntitySnapshot> entities;
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
