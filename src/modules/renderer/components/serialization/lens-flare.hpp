#pragma once

#include "components/lens-flare.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const rendering::LensFlare &lens_flare) {
  ComponentSnapshot snapshot{.name = "LensFlare"};
  snapshot.fields.push_back({"enabled", lens_flare.enabled});
  snapshot.fields.push_back({"intensity", lens_flare.intensity});
  snapshot.fields.push_back({"threshold", lens_flare.threshold});
  snapshot.fields.push_back({"ghost_count", static_cast<float>(lens_flare.ghost_count)});
  snapshot.fields.push_back({"ghost_dispersal", lens_flare.ghost_dispersal});
  snapshot.fields.push_back({"ghost_weight", lens_flare.ghost_weight});
  snapshot.fields.push_back({"halo_radius", lens_flare.halo_radius});
  snapshot.fields.push_back({"halo_weight", lens_flare.halo_weight});
  snapshot.fields.push_back({"halo_thickness", lens_flare.halo_thickness});
  snapshot.fields.push_back({"chromatic_aberration", lens_flare.chromatic_aberration});
  return snapshot;
}

inline void apply_lens_flare_snapshot(
    ecs::EntityRef entity, const serialization::fields::FieldList &fields) {
  entity.emplace<rendering::LensFlare>(rendering::LensFlare{
      .enabled = serialization::fields::read_bool(fields, "enabled")
                     .value_or(true),
      .intensity = serialization::fields::read_float(fields, "intensity")
                       .value_or(1.0f),
      .threshold = serialization::fields::read_float(fields, "threshold")
                       .value_or(0.8f),
      .ghost_count = static_cast<int>(
          serialization::fields::read_float(fields, "ghost_count")
              .value_or(4.0f)),
      .ghost_dispersal = serialization::fields::read_float(fields, "ghost_dispersal")
                             .value_or(0.35f),
      .ghost_weight = serialization::fields::read_float(fields, "ghost_weight")
                          .value_or(0.5f),
      .halo_radius = serialization::fields::read_float(fields, "halo_radius")
                         .value_or(0.6f),
      .halo_weight = serialization::fields::read_float(fields, "halo_weight")
                         .value_or(0.25f),
      .halo_thickness = serialization::fields::read_float(fields, "halo_thickness")
                            .value_or(0.1f),
      .chromatic_aberration =
          serialization::fields::read_float(fields, "chromatic_aberration")
              .value_or(0.02f),
  });
}

} // namespace astralix::serialization
