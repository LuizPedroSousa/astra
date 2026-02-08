#include "mesh-collector.hpp"

#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "entities/object.hpp"
#include "glad//glad.h"
#include "guid.hpp"
#include "managers/entity-manager.hpp"
#include "renderer-api.hpp"
#include "storage-buffer.hpp"
#include "trace.hpp"
#include <cstdint>

#include <unordered_map>

namespace astralix {

MeshGroupID MeshCollector::compute_group_id(std::vector<Mesh> &meshes,
                                            uint32_t shader_id) {
  ASTRA_PROFILE_N("MeshCollector Compute Group ID");

  MeshGroupID group_id = shader_id;

  for (auto mesh : meshes) {
    group_id ^= mesh.id;
  }

  return group_id;
}
} // namespace astralix
