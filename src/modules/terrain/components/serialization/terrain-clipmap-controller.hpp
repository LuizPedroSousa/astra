#pragma once

#include "components/terrain-clipmap-controller.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const terrain::TerrainClipmapController &controller) {
  ComponentSnapshot snapshot{.name = "TerrainClipmapController"};
  snapshot.fields.push_back({"levels", static_cast<int>(controller.levels)});
  snapshot.fields.push_back({"ring_vertices", static_cast<int>(controller.ring_vertices)});
  snapshot.fields.push_back({"base_ring_radius", controller.base_ring_radius});
  snapshot.fields.push_back({"enabled", controller.enabled});
  return snapshot;
}

inline void apply_terrain_clipmap_controller_snapshot(ecs::EntityRef entity,
                                                      const serialization::fields::FieldList &fields) {
  entity.emplace<terrain::TerrainClipmapController>(terrain::TerrainClipmapController{
      .levels = static_cast<uint32_t>(
          serialization::fields::read_int(fields, "levels").value_or(6)),
      .ring_vertices = static_cast<uint32_t>(
          serialization::fields::read_int(fields, "ring_vertices").value_or(64)),
      .base_ring_radius =
          serialization::fields::read_float(fields, "base_ring_radius").value_or(32.0f),
      .enabled = serialization::fields::read_bool(fields, "enabled").value_or(true),
  });
}

} // namespace astralix::serialization
