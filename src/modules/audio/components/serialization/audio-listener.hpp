#pragma once

#include "components/audio-listener.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const audio::AudioListener &listener) {
  ComponentSnapshot snapshot{.name = "AudioListener"};
  snapshot.fields.push_back({"enabled", listener.enabled});
  snapshot.fields.push_back({"gain", listener.gain});
  return snapshot;
}

inline void apply_audio_listener_snapshot(ecs::EntityRef entity,
                                          const serialization::fields::FieldList &fields) {
  entity.emplace<audio::AudioListener>(audio::AudioListener{
      .enabled = serialization::fields::read_bool(fields, "enabled").value_or(true),
      .gain = serialization::fields::read_float(fields, "gain").value_or(1.0f),
  });
}

} // namespace astralix::serialization
