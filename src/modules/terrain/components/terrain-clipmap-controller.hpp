#pragma once

#include <cstdint>

namespace astralix::terrain {

struct TerrainClipmapController {
  uint32_t levels = 6;
  uint32_t ring_vertices = 64;
  float base_ring_radius = 32.0f;
  bool enabled = true;
};

} // namespace astralix::terrain
