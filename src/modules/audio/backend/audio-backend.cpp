#include "audio-backend.hpp"
#include "log.hpp"

namespace astralix::audio {

  bool AudioBackend::initialize(uint32_t listener_count) {
    ma_engine_config config = ma_engine_config_init();
    config.listenerCount = listener_count;

    ma_result result = ma_engine_init(&config, &m_engine);
    if (result != MA_SUCCESS) {
      LOG_ERROR("[AUDIO BACKEND] failed to initialize miniaudio engine (error ", result, ")");
      return false;
    }

    m_initialized = true;
    return true;
  }

  void AudioBackend::shutdown() {
    if (m_initialized) {
      ma_engine_uninit(&m_engine);
      m_initialized = false;
    }
  }

  bool AudioBackend::is_initialized() const { return m_initialized; }

  void AudioBackend::set_master_volume(float gain) {
    ma_engine_set_volume(&m_engine, gain);
  }

  void AudioBackend::set_listener_position(uint32_t index, const glm::vec3& position) {
    ma_engine_listener_set_position(&m_engine, index, position.x, position.y, position.z);
  }

  void AudioBackend::set_listener_direction(uint32_t index, const glm::vec3& forward,
    const glm::vec3& up) {
    ma_engine_listener_set_direction(&m_engine, index, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&m_engine, index, up.x, up.y, up.z);
  }

  Scope<VoiceHandle> AudioBackend::create_voice(const std::string& file_path, bool spatial) {
    auto handle = std::make_unique<VoiceHandle>();

    ma_result result = ma_sound_init_from_file(
      &m_engine, file_path.c_str(), MA_SOUND_FLAG_DECODE,
      nullptr, nullptr, &handle->sound);

    if (result != MA_SUCCESS) {
      LOG_ERROR("[AUDIO BACKEND] failed to load '", file_path, "' (error ", result, ")");
      return nullptr;
    }

    handle->initialized = true;
    ma_sound_set_spatialization_enabled(&handle->sound, spatial ? MA_TRUE : MA_FALSE);
    return handle;
  }

  void AudioBackend::destroy_voice(VoiceHandle& handle) {
    if (handle.initialized) {
      ma_sound_uninit(&handle.sound);
      handle.initialized = false;
    }
  }

  void AudioBackend::voice_set_position(VoiceHandle& handle, const glm::vec3& position) {
    ma_sound_set_position(&handle.sound, position.x, position.y, position.z);
  }

  void AudioBackend::voice_set_volume(VoiceHandle& handle, float gain) {
    ma_sound_set_volume(&handle.sound, gain);
  }

  void AudioBackend::voice_set_pitch(VoiceHandle& handle, float pitch) {
    ma_sound_set_pitch(&handle.sound, pitch);
  }

  void AudioBackend::voice_set_looping(VoiceHandle& handle, bool looping) {
    ma_sound_set_looping(&handle.sound, looping ? MA_TRUE : MA_FALSE);
  }

  void AudioBackend::voice_set_min_distance(VoiceHandle& handle, float distance) {
    ma_sound_set_min_distance(&handle.sound, distance);
  }

  void AudioBackend::voice_set_max_distance(VoiceHandle& handle, float distance) {
    ma_sound_set_max_distance(&handle.sound, distance);
  }

  void AudioBackend::voice_set_rolloff(VoiceHandle& handle, float rolloff) {
    ma_sound_set_rolloff(&handle.sound, rolloff);
  }

  void AudioBackend::voice_start(VoiceHandle& handle) {
    ma_sound_start(&handle.sound);
  }

  void AudioBackend::voice_stop(VoiceHandle& handle) {
    ma_sound_stop(&handle.sound);
  }

  bool AudioBackend::voice_is_playing(const VoiceHandle& handle) const {
    return ma_sound_is_playing(&handle.sound);
  }

  bool AudioBackend::voice_at_end(const VoiceHandle& handle) const {
    return ma_sound_at_end(&handle.sound);
  }

} // namespace astralix::audio
