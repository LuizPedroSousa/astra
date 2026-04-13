#pragma once

#include "graph/audio-pass.hpp"

namespace astralix::audio {

class SpatialUpdatePass : public AudioPass {
public:
  void process(AudioFrame &frame, AudioBackend &backend) override;
  std::string_view name() const override { return "SpatialUpdate"; }
  std::span<const FrameField> reads() const override { return s_reads; }
  std::span<const FrameField> writes() const override { return {}; }

private:
  static constexpr FrameField s_reads[] = {
      FrameField::Listener, FrameField::Emitters, FrameField::Voices};
};

} // namespace astralix::audio
