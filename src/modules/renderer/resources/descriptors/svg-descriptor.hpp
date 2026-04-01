#pragma once

#include "base.hpp"
#include "guid.hpp"
#include "path.hpp"
#include "resources/descriptors/resource-descriptor.hpp"

namespace astralix {

struct SvgDescriptor {
public:
  SvgDescriptor(const ResourceDescriptorID &id, Ref<Path> path)
      : RESOURCE_DESCRIPTOR_INIT(), path(std::move(path)) {}

  static Ref<SvgDescriptor> create(const ResourceDescriptorID &id, Ref<Path> path);

  RESOURCE_DESCRIPTOR_PARAMS;

  Ref<Path> path;
};

} // namespace astralix
