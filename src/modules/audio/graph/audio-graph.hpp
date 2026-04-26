#pragma once

#include "audio-pass.hpp"
#include "base.hpp"
#include <vector>

namespace astralix::audio {

class AudioGraph {
public:
  void add_pass(Scope<AudioPass> pass);
  void compile();
  void setup(AudioFrame &frame, AudioBackend &backend);
  void process(AudioFrame &frame, AudioBackend &backend);
  void teardown(AudioFrame &frame, AudioBackend &backend);

  std::span<const Scope<AudioPass>> passes() const;

private:
  std::vector<Scope<AudioPass>> m_passes;
  bool m_compiled = false;
};

} // namespace astralix::audio
