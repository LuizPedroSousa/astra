#pragma once

#include "components/skybox.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const rendering::SkyboxBinding &skybox) {
  ComponentSnapshot snapshot{.name = "SkyboxBinding"};
  snapshot.fields.push_back({"cubemap", skybox.cubemap});
  snapshot.fields.push_back({"shader", skybox.shader});
  return snapshot;
}

inline void apply_skybox_snapshot(ecs::EntityRef entity,
                                  const serialization::fields::FieldList
                                      &fields) {
  entity.emplace<rendering::SkyboxBinding>(rendering::SkyboxBinding{
      .cubemap = serialization::fields::read_string(fields, "cubemap")
                     .value_or(""),
      .shader =
          serialization::fields::read_string(fields, "shader").value_or(""),
  });
}

} // namespace astralix::serialization
