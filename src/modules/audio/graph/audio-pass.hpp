#pragma once

#include "audio-frame.hpp"
#include "backend/audio-backend.hpp"
#include <span>
#include <string_view>

namespace astralix::audio {

class AudioPass {
public:
  virtual ~AudioPass() = default;

  virtual void setup(AudioFrame &frame, AudioBackend &backend) {}
  virtual void process(AudioFrame &frame, AudioBackend &backend) = 0;
  virtual void teardown(AudioFrame &frame, AudioBackend &backend) {}

  virtual std::string_view name() const = 0;
  virtual std::span<const FrameField> reads() const = 0;
  virtual std::span<const FrameField> writes() const = 0;

  bool enabled = true;
};

} // namespace astralix::audio
