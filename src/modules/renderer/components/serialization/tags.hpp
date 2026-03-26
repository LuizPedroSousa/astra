#pragma once

#include "components/tags.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "world.hpp"

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const scene::SceneEntity &) {
  return ComponentSnapshot{.name = "SceneEntity"};
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
