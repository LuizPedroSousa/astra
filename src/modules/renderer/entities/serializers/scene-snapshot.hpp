#pragma once

#include "scene-component-serialization.hpp"
#include "world.hpp"
#include <vector>

namespace astralix::serialization {

inline std::vector<EntitySnapshot> collect_scene_snapshots(const ecs::World &world) {
  std::vector<EntitySnapshot> snapshots;

  world.each<scene::SceneEntity>([&](EntityID entity_id, const scene::SceneEntity &) {
    auto entity = const_cast<ecs::World &>(world).entity(entity_id);
    EntitySnapshot snapshot{
        .id = entity_id,
        .name = std::string(entity.name()),
        .active = entity.active(),
    };

    append_snapshot_if_present<scene::SceneEntity>(entity, snapshot.components);
    append_snapshot_if_present<scene::Transform>(entity, snapshot.components);
    append_snapshot_if_present<rendering::Camera>(entity, snapshot.components);
    append_snapshot_if_present<scene::CameraController>(entity, snapshot.components);
    append_snapshot_if_present<rendering::Light>(entity, snapshot.components);
    append_snapshot_if_present<rendering::PointLightAttenuation>(entity,
                                                      snapshot.components);
    append_snapshot_if_present<rendering::SpotLightCone>(entity, snapshot.components);
    append_snapshot_if_present<rendering::DirectionalShadowSettings>(
        entity, snapshot.components);
    append_snapshot_if_present<rendering::SpotLightTarget>(entity, snapshot.components);
    append_snapshot_if_present<rendering::ModelRef>(entity, snapshot.components);
    append_snapshot_if_present<rendering::MeshSet>(entity, snapshot.components);
    append_snapshot_if_present<rendering::MaterialSlots>(entity, snapshot.components);
    append_snapshot_if_present<rendering::ShaderBinding>(entity, snapshot.components);
    append_snapshot_if_present<rendering::TextureBindings>(entity, snapshot.components);
    append_snapshot_if_present<rendering::SkyboxBinding>(entity, snapshot.components);
    append_snapshot_if_present<rendering::TextSprite>(entity, snapshot.components);
    append_snapshot_if_present<physics::RigidBody>(entity, snapshot.components);
    append_snapshot_if_present<physics::BoxCollider>(entity, snapshot.components);
    append_snapshot_if_present<physics::FitBoxColliderFromRenderMesh>(
        entity, snapshot.components);
    append_snapshot_if_present<rendering::Renderable>(entity, snapshot.components);
    append_snapshot_if_present<rendering::MainCamera>(entity, snapshot.components);
    append_snapshot_if_present<rendering::ShadowCaster>(entity, snapshot.components);

    snapshots.push_back(std::move(snapshot));
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
