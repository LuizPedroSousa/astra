#pragma once

#include "components/collider.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const physics::BoxCollider &collider) {
  ComponentSnapshot snapshot{.name = "BoxCollider"};
  serialization::fields::append_vec3(snapshot.fields, "half_extents",
                                     collider.half_extents);
  serialization::fields::append_vec3(snapshot.fields, "center",
                                     collider.center);
  return snapshot;
}

inline ComponentSnapshot
snapshot_component(const physics::FitBoxColliderFromRenderMesh &) {
  return ComponentSnapshot{.name = "FitBoxColliderFromRenderMesh"};
}

inline void apply_box_collider_snapshot(
    ecs::EntityRef entity, const serialization::fields::FieldList &fields) {
  entity.emplace<physics::BoxCollider>(physics::BoxCollider{
      .half_extents = serialization::fields::read_vec3(
          fields, "half_extents", glm::vec3(0.5f)),
      .center = serialization::fields::read_vec3(fields, "center"),
  });
}

inline void apply_fit_box_collider_from_render_mesh_snapshot(ecs::EntityRef entity) {
  entity.emplace<physics::FitBoxColliderFromRenderMesh>();
}

} // namespace astralix::serialization
