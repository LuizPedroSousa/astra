#pragma once

#include "components/model.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const rendering::ModelRef &model_ref) {
  ComponentSnapshot snapshot{.name = "ModelRef"};
  for (size_t i = 0; i < model_ref.resource_ids.size(); ++i) {
    snapshot.fields.push_back(
        {"model_" + std::to_string(i), model_ref.resource_ids[i]});
  }
  return snapshot;
}

inline void apply_model_ref_snapshot(ecs::EntityRef entity,
                                     const serialization::fields::FieldList
                                         &fields) {
  entity.emplace<rendering::ModelRef>(
      rendering::ModelRef{.resource_ids =
                   serialization::fields::read_string_series(fields, "model_")});
}

} // namespace astralix::serialization
