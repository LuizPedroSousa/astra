#pragma once

#include "base.hpp"
#include "guid.hpp"
#include "path.hpp"
#include "resources/descriptors/resource-descriptor.hpp"

namespace astralix {

struct AudioClipDescriptor {
  static Ref<AudioClipDescriptor> create(const ResourceDescriptorID &id,
                                         Ref<Path> path);

  AudioClipDescriptor(const ResourceDescriptorID &id, Ref<Path> path)
      : RESOURCE_DESCRIPTOR_INIT(), path(path) {}

  RESOURCE_DESCRIPTOR_PARAMS;

  Ref<Path> path;
};

} // namespace astralix
