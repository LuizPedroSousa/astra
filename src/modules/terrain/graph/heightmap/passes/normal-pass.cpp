#include "normal-pass.hpp"
#include "trace.hpp"
#include <algorithm>
#include <cmath>

namespace astralix::terrain {

void NormalPass::process(HeightmapFrame &frame, const TerrainRecipeData &recipe) {
  ASTRA_PROFILE_N("NormalPass::process");

  uint32_t resolution = frame.resolution;
  if (resolution == 0) return;

  float texel_size = frame.tile_world_size / static_cast<float>(resolution - 1);

  for (uint32_t y = 0; y < resolution; ++y) {
    for (uint32_t x = 0; x < resolution; ++x) {
      uint32_t xm = x > 0 ? x - 1 : 0;
      uint32_t xp = x < resolution - 1 ? x + 1 : resolution - 1;
      uint32_t ym = y > 0 ? y - 1 : 0;
      uint32_t yp = y < resolution - 1 ? y + 1 : resolution - 1;

      float height_left = frame.heightmap[y * resolution + xm] * frame.height_scale;
      float height_right = frame.heightmap[y * resolution + xp] * frame.height_scale;
      float height_down = frame.heightmap[ym * resolution + x] * frame.height_scale;
      float height_up = frame.heightmap[yp * resolution + x] * frame.height_scale;
      float height_center = frame.heightmap[y * resolution + x] * frame.height_scale;

      glm::vec3 normal = glm::normalize(glm::vec3(
          (height_left - height_right) / (2.0f * texel_size),
          1.0f,
          (height_down - height_up) / (2.0f * texel_size)
      ));

      float curvature = (height_left + height_right + height_down + height_up - 4.0f * height_center) / (texel_size * texel_size);
      curvature = std::clamp(curvature * 0.01f + 0.5f, 0.0f, 1.0f);

      frame.normal_map[y * resolution + x] = glm::vec4(normal, curvature);
    }
  }
}

} // namespace astralix::terrain
