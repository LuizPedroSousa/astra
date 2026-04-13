#include "one-shot-pass.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/audio-clip-descriptor.hpp"

namespace astralix::audio {

void OneShotPass::process(AudioFrame &frame, AudioBackend &backend) {
  std::erase_if(frame.one_shots, [&](const std::unique_ptr<VoiceHandle> &handle) {
    if (backend.voice_at_end(*handle)) {
      backend.destroy_voice(*handle);
      return true;
    }
    return false;
  });

  for (auto &request : frame.one_shot_queue) {
    auto descriptor =
        resource_manager()->get_descriptor_by_id<AudioClipDescriptor>(request.clip_id);

    if (descriptor == nullptr) {
      LOG_ERROR("[ONE SHOT] unknown clip_id '", request.clip_id, "'");
      continue;
    }

    std::string resolved_path = path_manager()->resolve(descriptor->path).string();
    auto handle = backend.create_voice(resolved_path, request.spatial);

    if (handle == nullptr) {
      continue;
    }

    if (request.spatial) {
      backend.voice_set_position(*handle, request.position);
    }

    backend.voice_set_volume(*handle, request.gain);
    backend.voice_set_pitch(*handle, request.pitch);
    backend.voice_start(*handle);

    frame.one_shots.push_back(std::move(handle));
  }

  frame.one_shot_queue.clear();
}

void OneShotPass::teardown(AudioFrame &frame, AudioBackend &backend) {
  for (auto &handle : frame.one_shots) {
    backend.destroy_voice(*handle);
  }
  frame.one_shots.clear();
}

} // namespace astralix::audio
