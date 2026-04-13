#include "audio-commands.hpp"
#include "graph/audio-frame.hpp"

namespace astralix::audio {

static std::vector<OneShotRequest> g_one_shot_queue;

std::vector<OneShotRequest> &get_one_shot_queue() {
  return g_one_shot_queue;
}

void queue_audio_one_shot(const std::string &clip_id, const glm::vec3 &position,
                          float gain, float pitch) {
  g_one_shot_queue.push_back({
      .clip_id = clip_id,
      .position = position,
      .gain = gain,
      .pitch = pitch,
      .spatial = true,
  });
}

void queue_audio_one_shot_2d(const std::string &clip_id,
                             float gain, float pitch) {
  g_one_shot_queue.push_back({
      .clip_id = clip_id,
      .gain = gain,
      .pitch = pitch,
      .spatial = false,
  });
}

} // namespace astralix::audio
