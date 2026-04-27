#pragma once

#include <string>

namespace astralix::terrain {

struct TerrainTile {
  std::string recipe_id;
  bool enabled = true;
  float height_scale = 64.0f;
  float uv_scale = 1.0f;
};

} // namespace astralix::terrain
