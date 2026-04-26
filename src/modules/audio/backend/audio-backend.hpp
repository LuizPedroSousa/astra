#pragma once

#include "base.hpp"
#include "miniaudio.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace astralix::audio {

  struct VoiceHandle {
    ma_sound sound;
    bool initialized = false;
  };

  class AudioBackend {
  public:
    bool initialize(uint32_t listener_count = 1);
    void shutdown();
    bool is_initialized() const;

    void set_master_volume(float gain);

    void set_listener_position(uint32_t index, const glm::vec3& position);
    void set_listener_direction(uint32_t index, const glm::vec3& forward, const glm::vec3& up);

    Scope<VoiceHandle> create_voice(const std::string& file_path, bool spatial);
    void destroy_voice(VoiceHandle& handle);

    void voice_set_position(VoiceHandle& handle, const glm::vec3& position);
    void voice_set_volume(VoiceHandle& handle, float gain);
    void voice_set_pitch(VoiceHandle& handle, float pitch);
    void voice_set_looping(VoiceHandle& handle, bool looping);
    void voice_set_min_distance(VoiceHandle& handle, float distance);
    void voice_set_max_distance(VoiceHandle& handle, float distance);
    void voice_set_rolloff(VoiceHandle& handle, float rolloff);

    void voice_start(VoiceHandle& handle);
    void voice_stop(VoiceHandle& handle);
    bool voice_is_playing(const VoiceHandle& handle) const;
    bool voice_at_end(const VoiceHandle& handle) const;

  private:
    ma_engine m_engine{};
    bool m_initialized = false;
  };

} // namespace astralix::audio
