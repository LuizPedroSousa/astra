#include "resources/descriptors/material-descriptor.hpp"
#include "base.hpp"
#include "guid.hpp"

namespace astralix {

Ref<MaterialDescriptor> MaterialDescriptor::create(
    const ResourceDescriptorID &id,
    std::vector<ResourceDescriptorID> diffuse_ids,
    std::vector<ResourceDescriptorID> specular_ids,
    std::optional<ResourceDescriptorID> normal_map_ids,
    std::optional<ResourceDescriptorID> displacement_map_ids) {
  return create_ref<MaterialDescriptor>(id, diffuse_ids, specular_ids,
                                        normal_map_ids, displacement_map_ids);
}

} // namespace astralix
