#pragma once

#include <glm/glm.hpp>
#include <string>

namespace astralix::audio {

void queue_audio_one_shot(const std::string &clip_id, const glm::vec3 &position,
                          float gain = 1.0f, float pitch = 1.0f);

void queue_audio_one_shot_2d(const std::string &clip_id,
                             float gain = 1.0f, float pitch = 1.0f);

} // namespace astralix::audio
