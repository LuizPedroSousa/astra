#include "resources/descriptors/font-descriptor.hpp"
#include "base.hpp"
#include "guid.hpp"

namespace astralix {

Ref<FontDescriptor> FontDescriptor::create(const ResourceDescriptorID &id,
                                           Ref<Path> path) {
  return create_ref<FontDescriptor>(id, path);
}

} // namespace astralix
