#pragma once

#include "components/tags.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const scene::SceneEntity &) {
  return ComponentSnapshot{.name = "SceneEntity"};
}

inline ComponentSnapshot snapshot_component(const scene::EditorOnly &) {
  return ComponentSnapshot{.name = "EditorOnly"};
}

inline ComponentSnapshot snapshot_component(const scene::GeneratorSpec &) {
  return ComponentSnapshot{.name = "GeneratorSpec"};
}

inline ComponentSnapshot snapshot_component(const scene::DerivedEntity &) {
  return ComponentSnapshot{.name = "DerivedEntity"};
}

inline ComponentSnapshot
snapshot_component(const scene::MetaEntityOwner &owner) {
  ComponentSnapshot snapshot{.name = "MetaEntityOwner"};
  snapshot.fields.push_back({"generator_id", owner.generator_id});
  snapshot.fields.push_back({"stable_key", owner.stable_key});
  return snapshot;
}

inline ComponentSnapshot snapshot_component(const rendering::Renderable &) {
  return ComponentSnapshot{.name = "Renderable"};
}

inline ComponentSnapshot snapshot_component(const rendering::MainCamera &) {
  return ComponentSnapshot{.name = "MainCamera"};
}

inline ComponentSnapshot snapshot_component(const rendering::ShadowCaster &) {
  return ComponentSnapshot{.name = "ShadowCaster"};
}

inline void apply_scene_entity_snapshot(ecs::EntityRef entity) {
  entity.emplace<scene::SceneEntity>();
}

inline void apply_editor_only_snapshot(ecs::EntityRef entity) {
  entity.emplace<scene::EditorOnly>();
}

inline void apply_generator_spec_snapshot(ecs::EntityRef entity) {
  entity.emplace<scene::GeneratorSpec>();
}

inline void apply_derived_entity_snapshot(ecs::EntityRef entity) {
  entity.emplace<scene::DerivedEntity>();
}

inline void apply_meta_entity_owner_snapshot(
    ecs::EntityRef entity,
    const serialization::fields::FieldList &fields
) {
  entity.emplace<scene::MetaEntityOwner>(scene::MetaEntityOwner{
      .generator_id = serialization::fields::read_string(fields, "generator_id")
                          .value_or(""),
      .stable_key =
          serialization::fields::read_string(fields, "stable_key").value_or(""),
  });
}

inline void apply_renderable_snapshot(ecs::EntityRef entity) {
  entity.emplace<rendering::Renderable>();
}

inline void apply_main_camera_snapshot(ecs::EntityRef entity) {
  entity.emplace<rendering::MainCamera>();
}

inline void apply_shadow_caster_snapshot(ecs::EntityRef entity) {
  entity.emplace<rendering::ShadowCaster>();
}

} // namespace astralix::serialization
