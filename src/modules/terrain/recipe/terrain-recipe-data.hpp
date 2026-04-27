#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astralix::terrain {

struct NoiseConfig {
  std::string type = "fbm";
  uint32_t seed = 42;
  uint32_t octaves = 6;
  float frequency = 0.005f;
  float lacunarity = 2.0f;
  float persistence = 0.5f;
  float amplitude = 1.0f;
};

struct ErosionConfig {
  uint32_t iterations = 50000;
  uint32_t drop_lifetime = 30;
  float inertia = 0.05f;
  float sediment_capacity = 4.0f;
  float min_sediment_capacity = 0.01f;
  float deposit_speed = 0.3f;
  float erode_speed = 0.3f;
  float evaporate_speed = 0.01f;
  float gravity = 4.0f;
  uint32_t erode_radius = 3;
};

struct SplatLayerConfig {
  std::string material_id;
  std::string channel;
  float min_slope = 0.0f;
  float max_slope = 1.0f;
  float min_height = 0.0f;
  float max_height = 1.0f;
};

struct SplatConfig {
  std::vector<SplatLayerConfig> layers;
};

struct TerrainRecipeData {
  uint32_t resolution = 1025;
  NoiseConfig noise;
  ErosionConfig erosion;
  SplatConfig splat;
};

TerrainRecipeData parse_terrain_recipe(const std::string &recipe_path);

} // namespace astralix::terrain
