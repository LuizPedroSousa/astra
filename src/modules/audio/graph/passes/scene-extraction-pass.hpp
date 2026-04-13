#pragma once

#include "graph/audio-pass.hpp"

namespace astralix {
class Scene;
}

namespace astralix::audio {

class SceneExtractionPass : public AudioPass {
public:
  void process(AudioFrame &frame, AudioBackend &backend) override;
  std::string_view name() const override { return "SceneExtraction"; }
  std::span<const FrameField> reads() const override { return {}; }
  std::span<const FrameField> writes() const override { return s_writes; }

private:
  static constexpr FrameField s_writes[] = {
      FrameField::Listener, FrameField::Emitters, FrameField::SceneState};

  Scene *m_tracked_scene = nullptr;
  uint64_t m_tracked_revision = 0;
};

} // namespace astralix::audio
