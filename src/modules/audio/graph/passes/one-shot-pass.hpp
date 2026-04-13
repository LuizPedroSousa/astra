#pragma once

#include "graph/audio-pass.hpp"

namespace astralix::audio {

class OneShotPass : public AudioPass {
public:
  void process(AudioFrame &frame, AudioBackend &backend) override;
  void teardown(AudioFrame &frame, AudioBackend &backend) override;
  std::string_view name() const override { return "OneShot"; }
  std::span<const FrameField> reads() const override { return s_reads; }
  std::span<const FrameField> writes() const override { return {}; }

private:
  static constexpr FrameField s_reads[] = {FrameField::OneShotQueue};
};

} // namespace astralix::audio
