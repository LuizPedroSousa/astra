#pragma once

#include "graph/audio-pass.hpp"

namespace astralix::audio {

class PlayStatePass : public AudioPass {
public:
  void process(AudioFrame &frame, AudioBackend &backend) override;
  std::string_view name() const override { return "PlayState"; }
  std::span<const FrameField> reads() const override { return s_reads; }
  std::span<const FrameField> writes() const override { return {}; }

private:
  static constexpr FrameField s_reads[] = {
      FrameField::Emitters, FrameField::Voices, FrameField::SceneState};
};

} // namespace astralix::audio
