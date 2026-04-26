#include "resources/descriptors/audio-clip-descriptor.hpp"
#include "base.hpp"

namespace astralix {

Ref<AudioClipDescriptor> AudioClipDescriptor::create(const ResourceDescriptorID &id,
                                                     Ref<Path> path) {
  return create_ref<AudioClipDescriptor>(id, path);
}

} // namespace astralix
