#include "splat-pass.hpp"
#include "trace.hpp"
#include <algorithm>
#include <cmath>

namespace astralix::terrain {

void SplatPass::process(HeightmapFrame &frame, const TerrainRecipeData &recipe) {
  ASTRA_PROFILE_N("SplatPass::process");

  uint32_t resolution = frame.resolution;
  if (resolution == 0) return;

  auto channel_index = [](const std::string &channel) -> int {
    if (channel == "r") return 0;
    if (channel == "g") return 1;
    if (channel == "b") return 2;
    if (channel == "a") return 3;
    return -1;
  };

  for (uint32_t y = 0; y < resolution; ++y) {
    for (uint32_t x = 0; x < resolution; ++x) {
      size_t idx = y * resolution + x;
      float height = frame.heightmap[idx];
      glm::vec3 normal = glm::vec3(frame.normal_map[idx]);
      float slope = 1.0f - normal.y;

      float weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};

      for (const auto &layer : recipe.splat.layers) {
        int channel = channel_index(layer.channel);
        if (channel < 0) continue;

        float slope_factor = 0.0f;
        if (slope >= layer.min_slope && slope <= layer.max_slope) {
          float slope_range = layer.max_slope - layer.min_slope;
          if (slope_range > 0.0f) {
            float slope_center = (layer.min_slope + layer.max_slope) * 0.5f;
            slope_factor = 1.0f - std::abs(slope - slope_center) / (slope_range * 0.5f);
          } else {
            slope_factor = 1.0f;
          }
        }

        float height_factor = 0.0f;
        if (height >= layer.min_height && height <= layer.max_height) {
          float height_range = layer.max_height - layer.min_height;
          if (height_range > 0.0f) {
            float height_center = (layer.min_height + layer.max_height) * 0.5f;
            height_factor = 1.0f - std::abs(height - height_center) / (height_range * 0.5f);
          } else {
            height_factor = 1.0f;
          }
        }

        weights[channel] += slope_factor * height_factor;
      }

      float total = weights[0] + weights[1] + weights[2] + weights[3];
      if (total > 0.0f) {
        for (int index = 0; index < 4; ++index) weights[index] /= total;
      } else {
        weights[0] = 1.0f;
      }

      frame.splat_map[idx] = glm::u8vec4(
          static_cast<uint8_t>(weights[0] * 255.0f),
          static_cast<uint8_t>(weights[1] * 255.0f),
          static_cast<uint8_t>(weights[2] * 255.0f),
          static_cast<uint8_t>(weights[3] * 255.0f)
      );
    }
  }
}

} // namespace astralix::terrain
