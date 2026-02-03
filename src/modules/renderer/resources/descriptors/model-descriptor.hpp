#pragma once

#include "base.hpp"
#include "guid.hpp"
#include "path.hpp"
#include "resources/descriptors/resource-descriptor.hpp"

namespace astralix {
struct ModelDescriptor {
public:
  static Ref<ModelDescriptor> create(const ResourceDescriptorID &id,
                                     Ref<Path> path);

  ModelDescriptor(const ResourceDescriptorID &id, Ref<Path> path)
      : RESOURCE_DESCRIPTOR_INIT(), path(path) {}

  RESOURCE_DESCRIPTOR_PARAMS;
  Ref<Path> path;
};

} // namespace astralix
