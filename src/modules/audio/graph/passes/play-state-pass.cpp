#include "play-state-pass.hpp"

namespace astralix::audio {

void PlayStatePass::process(AudioFrame &frame, AudioBackend &backend) {
  if (!frame.scene_state.active) {
    return;
  }

  for (const auto &emitter : frame.emitters) {
    auto iterator = frame.voices.find(emitter.entity_id);
    if (iterator == frame.voices.end()) {
      continue;
    }

    auto &handle = *iterator->second;

    if (frame.scene_state.playing) {
      if (!backend.voice_is_playing(handle) && emitter.play_on_awake) {
        backend.voice_start(handle);
      }
    } else {
      if (backend.voice_is_playing(handle)) {
        backend.voice_stop(handle);
      }
    }
  }
}

} // namespace astralix::audio
