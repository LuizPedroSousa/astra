#include "mesh-build-pass.hpp"
#include "trace.hpp"

namespace astralix::terrain {

void MeshBuildPass::process(HeightmapFrame &frame, const TerrainRecipeData &recipe) {
  ASTRA_PROFILE_N("MeshBuildPass::process");

  frame.clipmap_rings.clear();
  frame.clipmap_rings.reserve(m_clipmap_levels);

  uint32_t resolution = frame.resolution;
  if (resolution == 0) return;

  float world_size = frame.tile_world_size;
  float texel_world = world_size / static_cast<float>(resolution - 1);

  for (uint32_t level = 0; level < m_clipmap_levels; ++level) {
    ClipmapRing ring;
    ring.level = level;

    uint32_t step = 1u << level;
    uint32_t grid_resolution = m_ring_vertices;

    uint32_t inner_step = (level > 0) ? (1u << (level - 1)) : 0;

    for (uint32_t gy = 0; gy < grid_resolution; ++gy) {
      for (uint32_t gx = 0; gx < grid_resolution; ++gx) {
        uint32_t hx = gx * step;
        uint32_t hy = gy * step;

        if (hx >= resolution || hy >= resolution) continue;

        if (level > 0 && gx > 0 && gx < grid_resolution - 1 &&
            gy > 0 && gy < grid_resolution - 1) {
          uint32_t inner_grid = grid_resolution;
          uint32_t inner_gx = gx * step / inner_step;
          uint32_t inner_gy = gy * step / inner_step;
          if (inner_gx > 0 && inner_gx < inner_grid - 1 &&
              inner_gy > 0 && inner_gy < inner_grid - 1) {
            continue;
          }
        }

        float world_x = static_cast<float>(hx) * texel_world - world_size * 0.5f;
        float world_z = static_cast<float>(hy) * texel_world - world_size * 0.5f;
        float height = frame.heightmap[hy * resolution + hx] * frame.height_scale;

        ring.positions.push_back(glm::vec3(world_x, height, world_z));

        glm::vec4 normal_data = frame.normal_map[hy * resolution + hx];
        ring.normals.push_back(glm::vec3(normal_data));

        ring.uvs.push_back(glm::vec2(
            static_cast<float>(hx) / static_cast<float>(resolution - 1),
            static_cast<float>(hy) / static_cast<float>(resolution - 1)
        ));
      }
    }

    frame.clipmap_rings.push_back(std::move(ring));
  }
}

} // namespace astralix::terrain
