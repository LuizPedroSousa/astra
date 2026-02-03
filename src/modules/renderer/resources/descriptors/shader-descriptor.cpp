#include "resources/descriptors/shader-descriptor.hpp"
#include "base.hpp"
#include "guid.hpp"

namespace astralix {

Ref<ShaderDescriptor> ShaderDescriptor::create(const ResourceDescriptorID &id,
                                               Ref<Path> fragment_path,
                                               Ref<Path> vertex_path,
                                               Ref<Path> geometry_path) {
  return create_ref<ShaderDescriptor>(id, fragment_path, vertex_path,
                                      geometry_path);
}

} // namespace astralix
