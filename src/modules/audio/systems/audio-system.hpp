#pragma once

#include "backend/audio-backend.hpp"
#include "graph/audio-frame.hpp"
#include "graph/audio-graph.hpp"
#include "project.hpp"
#include "systems/system.hpp"

namespace astralix {

class AudioSystem : public System<AudioSystem> {
public:
  AudioSystem(AudioSystemConfig &config);
  ~AudioSystem();

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

  const audio::AudioGraph &graph() const;

private:
  float m_master_gain;
  audio::AudioBackend m_backend;
  audio::AudioGraph m_graph;
  audio::AudioFrame m_frame;
};

} // namespace astralix
