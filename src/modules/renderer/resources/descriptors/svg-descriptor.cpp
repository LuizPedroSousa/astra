#include "resources/descriptors/svg-descriptor.hpp"

namespace astralix {

Ref<SvgDescriptor> SvgDescriptor::create(
    const ResourceDescriptorID &id,
    Ref<Path> path
) {
  return create_ref<SvgDescriptor>(id, std::move(path));
}

} // namespace astralix
