#pragma once

#include "components/transform.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const scene::Transform &transform) {
  ComponentSnapshot snapshot{.name = "Transform"};
  serialization::fields::append_vec3(snapshot.fields, "position",
                                     transform.position);
  serialization::fields::append_vec3(snapshot.fields, "scale",
                                     transform.scale);
  serialization::fields::append_quat(snapshot.fields, "rotation",
                                     transform.rotation);
  snapshot.fields.push_back({"dirty", transform.dirty});
  return snapshot;
}

inline void apply_transform_snapshot(ecs::EntityRef entity,
                                     const serialization::fields::FieldList
                                         &fields) {
  entity.emplace<scene::Transform>(scene::Transform{
      .position = serialization::fields::read_vec3(fields, "position"),
      .scale = serialization::fields::read_vec3(fields, "scale",
                                                glm::vec3(1.0f)),
      .rotation = serialization::fields::read_quat(fields, "rotation"),
      .matrix = glm::mat4(1.0f),
      .dirty = true,
  });
}

} // namespace astralix::serialization
