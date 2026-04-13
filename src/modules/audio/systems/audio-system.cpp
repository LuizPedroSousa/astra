#include "audio-system.hpp"
#include "audio-commands.hpp"
#include "graph/passes/emitter-sync-pass.hpp"
#include "graph/passes/one-shot-pass.hpp"
#include "graph/passes/play-state-pass.hpp"
#include "graph/passes/scene-extraction-pass.hpp"
#include "graph/passes/spatial-update-pass.hpp"
#include "trace.hpp"

namespace astralix {

namespace audio {
std::vector<OneShotRequest> &get_one_shot_queue();
} // namespace audio

AudioSystem::AudioSystem(AudioSystemConfig &config)
    : m_master_gain(config.master_gain) {}

void AudioSystem::start() {
  ASTRA_PROFILE_N("AudioSystem::start");

  if (!m_backend.initialize()) {
    return;
  }

  m_backend.set_master_volume(m_master_gain);

  m_graph.add_pass(create_scope<audio::SceneExtractionPass>());
  m_graph.add_pass(create_scope<audio::EmitterSyncPass>());
  m_graph.add_pass(create_scope<audio::SpatialUpdatePass>());
  m_graph.add_pass(create_scope<audio::PlayStatePass>());
  m_graph.add_pass(create_scope<audio::OneShotPass>());
  m_graph.compile();
  m_graph.setup(m_frame, m_backend);
}

void AudioSystem::update(double dt) {
  (void)dt;
  ASTRA_PROFILE_N("AudioSystem::update");

  if (!m_backend.is_initialized()) {
    return;
  }

  m_frame.clear_transient();
  m_frame.one_shot_queue = std::move(audio::get_one_shot_queue());
  audio::get_one_shot_queue().clear();

  m_graph.process(m_frame, m_backend);
}

void AudioSystem::fixed_update(double fixed_dt) { (void)fixed_dt; }

void AudioSystem::pre_update(double dt) { (void)dt; }

const audio::AudioGraph &AudioSystem::graph() const { return m_graph; }

AudioSystem::~AudioSystem() {
  m_graph.teardown(m_frame, m_backend);
  m_backend.shutdown();
}

} // namespace astralix
