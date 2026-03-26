#pragma once

#include "guid.hpp"
#include "resources/mesh.hpp"
#include <vector>

namespace astralix::rendering {

struct MeshSet {
  std::vector<Mesh> meshes;
};

} // namespace astralix::rendering
