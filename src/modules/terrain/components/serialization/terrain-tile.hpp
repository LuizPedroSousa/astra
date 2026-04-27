#pragma once

#include "components/terrain-tile.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const terrain::TerrainTile &tile) {
  ComponentSnapshot snapshot{.name = "TerrainTile"};
  snapshot.fields.push_back({"recipe_id", tile.recipe_id});
  snapshot.fields.push_back({"enabled", tile.enabled});
  snapshot.fields.push_back({"height_scale", tile.height_scale});
  snapshot.fields.push_back({"uv_scale", tile.uv_scale});
  return snapshot;
}

inline void apply_terrain_tile_snapshot(ecs::EntityRef entity,
                                        const serialization::fields::FieldList &fields) {
  entity.emplace<terrain::TerrainTile>(terrain::TerrainTile{
      .recipe_id = serialization::fields::read_string(fields, "recipe_id").value_or(""),
      .enabled = serialization::fields::read_bool(fields, "enabled").value_or(true),
      .height_scale = serialization::fields::read_float(fields, "height_scale").value_or(64.0f),
      .uv_scale = serialization::fields::read_float(fields, "uv_scale").value_or(1.0f),
  });
}

} // namespace astralix::serialization
