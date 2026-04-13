#pragma once

#include "graph/audio-pass.hpp"

namespace astralix::audio {

class EmitterSyncPass : public AudioPass {
public:
  void process(AudioFrame &frame, AudioBackend &backend) override;
  void teardown(AudioFrame &frame, AudioBackend &backend) override;
  std::string_view name() const override { return "EmitterSync"; }
  std::span<const FrameField> reads() const override { return s_reads; }
  std::span<const FrameField> writes() const override { return s_writes; }

private:
  static constexpr FrameField s_reads[] = {FrameField::Emitters, FrameField::SceneState};
  static constexpr FrameField s_writes[] = {FrameField::Voices};
};

} // namespace astralix::audio
