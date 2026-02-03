#include "resources/descriptors/model-descriptor.hpp"
#include "base.hpp"
#include "guid.hpp"

namespace astralix {

Ref<ModelDescriptor> ModelDescriptor::create(const ResourceDescriptorID &id,
                                             Ref<Path> path) {
  return create_ref<ModelDescriptor>(id, path);
}

} // namespace astralix
