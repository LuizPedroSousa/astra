#include "emitter-sync-pass.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/audio-clip-descriptor.hpp"
#include <unordered_set>

namespace astralix::audio {

void EmitterSyncPass::process(AudioFrame &frame, AudioBackend &backend) {
  if (!frame.scene_state.active) {
    for (auto &[entity_id, handle] : frame.voices) {
      backend.destroy_voice(*handle);
    }
    frame.voices.clear();
    return;
  }

  if (frame.scene_state.scene_changed) {
    for (auto &[entity_id, handle] : frame.voices) {
      backend.destroy_voice(*handle);
    }
    frame.voices.clear();
  }

  std::unordered_set<EntityID> active_entities;
  active_entities.reserve(frame.emitters.size());
  for (const auto &emitter : frame.emitters) {
    active_entities.insert(emitter.entity_id);
  }

  std::vector<EntityID> stale_entities;
  for (const auto &[entity_id, handle] : frame.voices) {
    if (!active_entities.contains(entity_id)) {
      backend.destroy_voice(*handle);
      stale_entities.push_back(entity_id);
    }
  }
  for (EntityID entity_id : stale_entities) {
    frame.voices.erase(entity_id);
  }

  for (const auto &emitter : frame.emitters) {
    if (frame.voices.contains(emitter.entity_id)) {
      continue;
    }

    if (emitter.clip_id.empty()) {
      continue;
    }

    auto descriptor =
        resource_manager()->get_descriptor_by_id<AudioClipDescriptor>(emitter.clip_id);

    if (descriptor == nullptr) {
      LOG_ERROR("[EMITTER SYNC] unknown clip_id '", emitter.clip_id, "'");
      continue;
    }

    std::string resolved_path = path_manager()->resolve(descriptor->path).string();
    auto handle = backend.create_voice(resolved_path, emitter.spatial);

    if (handle == nullptr) {
      continue;
    }

    frame.voices.emplace(emitter.entity_id, std::move(handle));
  }
}

void EmitterSyncPass::teardown(AudioFrame &frame, AudioBackend &backend) {
  for (auto &[entity_id, handle] : frame.voices) {
    backend.destroy_voice(*handle);
  }
  frame.voices.clear();
}

} // namespace astralix::audio
