#pragma once

#include "backend/audio-backend.hpp"
#include "components/audio-emitter.hpp"
#include "components/audio-listener.hpp"
#include "components/transform.hpp"
#include "guid.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace astralix::audio {

enum class FrameField : uint8_t {
  Listener,
  Emitters,
  Voices,
  OneShotQueue,
  SceneState,
};

struct ListenerData {
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  float gain = 1.0f;
  bool valid = false;
};

struct EmitterData {
  EntityID entity_id;
  glm::vec3 position = glm::vec3(0.0f);
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

struct OneShotRequest {
  std::string clip_id;
  glm::vec3 position = glm::vec3(0.0f);
  float gain = 1.0f;
  float pitch = 1.0f;
  bool spatial = true;
};

struct SceneStateData {
  bool active = false;
  bool playing = false;
  uint64_t session_revision = 0;
  bool scene_changed = false;
};

struct AudioFrame {
  ListenerData listener;
  std::vector<EmitterData> emitters;
  std::unordered_map<EntityID, std::unique_ptr<VoiceHandle>> voices;
  std::vector<std::unique_ptr<VoiceHandle>> one_shots;
  std::vector<OneShotRequest> one_shot_queue;
  SceneStateData scene_state;

  void clear_transient() {
    listener = {};
    emitters.clear();
    scene_state = {};
  }
};

} // namespace astralix::audio
