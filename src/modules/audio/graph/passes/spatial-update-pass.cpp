#include "spatial-update-pass.hpp"

namespace astralix::audio {

void SpatialUpdatePass::process(AudioFrame &frame, AudioBackend &backend) {
  if (frame.listener.valid) {
    backend.set_listener_position(0, frame.listener.position);
    backend.set_listener_direction(0, frame.listener.forward, frame.listener.up);
  }

  for (const auto &emitter : frame.emitters) {
    auto iterator = frame.voices.find(emitter.entity_id);
    if (iterator == frame.voices.end()) {
      continue;
    }

    auto &handle = *iterator->second;
    backend.voice_set_position(handle, emitter.position);
    backend.voice_set_volume(handle, emitter.gain);
    backend.voice_set_pitch(handle, emitter.pitch);
    backend.voice_set_looping(handle, emitter.looping);
    backend.voice_set_min_distance(handle, emitter.min_distance);
    backend.voice_set_max_distance(handle, emitter.max_distance);
    backend.voice_set_rolloff(handle, emitter.rolloff);
  }
}

} // namespace astralix::audio
