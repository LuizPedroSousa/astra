#pragma once

#include "components/audio-emitter.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const audio::AudioEmitter &emitter) {
  ComponentSnapshot snapshot{.name = "AudioEmitter"};
  snapshot.fields.push_back({"clip_id", emitter.clip_id});
  snapshot.fields.push_back({"enabled", emitter.enabled});
  snapshot.fields.push_back({"play_on_awake", emitter.play_on_awake});
  snapshot.fields.push_back({"looping", emitter.looping});
  snapshot.fields.push_back({"spatial", emitter.spatial});
  snapshot.fields.push_back({"gain", emitter.gain});
  snapshot.fields.push_back({"pitch", emitter.pitch});
  snapshot.fields.push_back({"min_distance", emitter.min_distance});
  snapshot.fields.push_back({"max_distance", emitter.max_distance});
  snapshot.fields.push_back({"rolloff", emitter.rolloff});
  return snapshot;
}

inline void apply_audio_emitter_snapshot(ecs::EntityRef entity,
                                         const serialization::fields::FieldList &fields) {
  entity.emplace<audio::AudioEmitter>(audio::AudioEmitter{
      .clip_id = serialization::fields::read_string(fields, "clip_id").value_or(""),
      .enabled = serialization::fields::read_bool(fields, "enabled").value_or(true),
      .play_on_awake = serialization::fields::read_bool(fields, "play_on_awake").value_or(true),
      .looping = serialization::fields::read_bool(fields, "looping").value_or(false),
      .spatial = serialization::fields::read_bool(fields, "spatial").value_or(true),
      .gain = serialization::fields::read_float(fields, "gain").value_or(1.0f),
      .pitch = serialization::fields::read_float(fields, "pitch").value_or(1.0f),
      .min_distance = serialization::fields::read_float(fields, "min_distance").value_or(1.0f),
      .max_distance = serialization::fields::read_float(fields, "max_distance").value_or(100.0f),
      .rolloff = serialization::fields::read_float(fields, "rolloff").value_or(1.0f),
  });
}

} // namespace astralix::serialization
