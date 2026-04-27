#include "resources/descriptors/terrain-recipe-descriptor.hpp"
#include "base.hpp"

namespace astralix {

Ref<TerrainRecipeDescriptor> TerrainRecipeDescriptor::create(const ResourceDescriptorID &id,
                                                             Ref<Path> path) {
  return create_ref<TerrainRecipeDescriptor>(id, path);
}

} // namespace astralix
