#pragma once

#include "base.hpp"
#include "guid.hpp"
#include "managers/resource-manager.hpp"
#include "path.hpp"
#include "resources/descriptors/audio-clip-descriptor.hpp"

namespace astralix {

struct AudioClip {
  static Ref<AudioClipDescriptor> create(const ResourceDescriptorID &id,
                                         Ref<Path> path) {
    return resource_manager()->register_audio_clip(
        AudioClipDescriptor::create(id, path));
  }
};

} // namespace astralix
