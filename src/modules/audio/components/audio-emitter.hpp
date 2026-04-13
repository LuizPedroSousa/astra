#pragma once

#include <string>

namespace astralix::audio {

struct AudioEmitter {
  std::string clip_id;
  bool enabled = true;
  bool play_on_awake = true;
  bool looping = false;
  bool spatial = true;
  float gain = 1.0f;
  float pitch = 1.0f;
  float min_distance = 1.0f;
  float max_distance = 100.0f;
  float rolloff = 1.0f;
};

} // namespace astralix::audio
