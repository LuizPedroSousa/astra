#pragma once

#include "scene-component-serialization.hpp"
#include "world.hpp"
#include <vector>

namespace astralix::serialization {

inline std::vector<ComponentSnapshot>
collect_entity_component_snapshots(ecs::EntityRef entity) {
  std::vector<ComponentSnapshot> components;
  if (!entity.exists()) {
    return components;
  }

  append_snapshot_if_present<scene::SceneEntity>(entity, components);
  append_snapshot_if_present<scene::Transform>(entity, components);
  append_snapshot_if_present<rendering::Camera>(entity, components);
  append_snapshot_if_present<scene::CameraController>(entity, components);
  append_snapshot_if_present<rendering::Light>(entity, components);
  append_snapshot_if_present<rendering::PointLightAttenuation>(entity, components);
  append_snapshot_if_present<rendering::SpotLightCone>(entity, components);
  append_snapshot_if_present<rendering::DirectionalShadowSettings>(
      entity, components
  );
  append_snapshot_if_present<rendering::SpotLightTarget>(entity, components);
  append_snapshot_if_present<rendering::ModelRef>(entity, components);
  append_snapshot_if_present<rendering::MeshSet>(entity, components);
  append_snapshot_if_present<rendering::MaterialSlots>(entity, components);
  append_snapshot_if_present<rendering::ShaderBinding>(entity, components);
  append_snapshot_if_present<rendering::TextureBindings>(entity, components);
  append_snapshot_if_present<rendering::BloomSettings>(entity, components);
  append_snapshot_if_present<rendering::SkyboxBinding>(entity, components);
  append_snapshot_if_present<rendering::TextSprite>(entity, components);
  append_snapshot_if_present<physics::RigidBody>(entity, components);
  append_snapshot_if_present<physics::BoxCollider>(entity, components);
  append_snapshot_if_present<physics::FitBoxColliderFromRenderMesh>(
      entity, components
  );
  append_snapshot_if_present<rendering::Renderable>(entity, components);
  append_snapshot_if_present<rendering::MainCamera>(entity, components);
  append_snapshot_if_present<rendering::ShadowCaster>(entity, components);
  return components;
}

inline EntitySnapshot collect_entity_snapshot(ecs::EntityRef entity) {
  EntitySnapshot snapshot{};
  if (!entity.exists()) {
    return snapshot;
  }

  snapshot.id = entity.id();
  snapshot.name = std::string(entity.name());
  snapshot.active = entity.active();
  snapshot.components = collect_entity_component_snapshots(entity);
  return snapshot;
}

inline std::vector<EntitySnapshot> collect_scene_snapshots(const ecs::World &world) {
  std::vector<EntitySnapshot> snapshots;

  world.each<scene::SceneEntity>([&](EntityID entity_id, const scene::SceneEntity &) {
    snapshots.push_back(
        collect_entity_snapshot(const_cast<ecs::World &>(world).entity(entity_id))
    );
  });

  return snapshots;
}

inline void
apply_scene_snapshots(ecs::World &world,
                      const std::vector<EntitySnapshot> &snapshots) {
  world = ecs::World();

  for (const auto &snapshot : snapshots) {
    auto entity = world.ensure(snapshot.id, snapshot.name, snapshot.active);
    for (const auto &component : snapshot.components) {
      apply_component_snapshot(entity, component);
    }
  }
}

} // namespace astralix::serialization
