#pragma once

#include "base.hpp"
#include "guid.hpp"
#include "managers/resource-manager.hpp"
#include "path.hpp"
#include "resources/descriptors/terrain-recipe-descriptor.hpp"

namespace astralix {

struct TerrainRecipe {
  static Ref<TerrainRecipeDescriptor> create(const ResourceDescriptorID &id,
                                             Ref<Path> path) {
    return resource_manager()->register_terrain_recipe(
        TerrainRecipeDescriptor::create(id, path));
  }
};

} // namespace astralix
