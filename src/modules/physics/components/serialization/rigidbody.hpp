#pragma once

#include "components/rigidbody.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"
#include <string>

namespace astralix::serialization {

inline std::string rigid_body_mode_to_string(physics::RigidBodyMode mode) {
  switch (mode) {
    case physics::RigidBodyMode::Static:
      return "static";
    case physics::RigidBodyMode::Dynamic:
      return "dynamic";
  }

  return "unknown";
}

inline physics::RigidBodyMode rigid_body_mode_from_string(const std::string &mode) {
  if (mode == "static") {
    return physics::RigidBodyMode::Static;
  }

  return physics::RigidBodyMode::Dynamic;
}

inline ComponentSnapshot snapshot_component(const physics::RigidBody &rigid_body) {
  ComponentSnapshot snapshot{.name = "RigidBody"};
  snapshot.fields.push_back({"mode", rigid_body_mode_to_string(rigid_body.mode)});
  snapshot.fields.push_back({"gravity", rigid_body.gravity});
  snapshot.fields.push_back({"velocity", rigid_body.velocity});
  snapshot.fields.push_back({"acceleration", rigid_body.acceleration});
  snapshot.fields.push_back({"drag", rigid_body.drag});
  snapshot.fields.push_back({"mass", rigid_body.mass});
  return snapshot;
}

inline void apply_rigid_body_snapshot(ecs::EntityRef entity,
                                      const serialization::fields::FieldList
                                          &fields) {
  entity.emplace<physics::RigidBody>(physics::RigidBody{
      .mode = rigid_body_mode_from_string(
          serialization::fields::read_string(fields, "mode")
              .value_or("dynamic")),
      .gravity = serialization::fields::read_float(fields, "gravity")
                     .value_or(0.5f),
      .velocity = serialization::fields::read_float(fields, "velocity")
                      .value_or(2.0f),
      .acceleration = serialization::fields::read_float(fields, "acceleration")
                          .value_or(2.0f),
      .drag =
          serialization::fields::read_float(fields, "drag").value_or(0.0f),
      .mass =
          serialization::fields::read_float(fields, "mass").value_or(1.0f),
  });
}

} // namespace astralix::serialization
