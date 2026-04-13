#pragma once

#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace astralix::terrain {

enum class HeightmapField : uint8_t {
  Heightmap,
  ErosionMap,
  NormalMap,
  SplatMap,
  MeshData,
};

struct ClipmapRing {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> uvs;
  std::vector<uint32_t> indices;
  uint32_t level = 0;
};

struct HeightmapFrame {
  uint32_t resolution = 0;
  float height_scale = 64.0f;
  float tile_world_size = 256.0f;

  std::vector<float> heightmap;
  std::vector<float> erosion_map;
  std::vector<glm::vec4> normal_map;
  std::vector<glm::u8vec4> splat_map;
  std::vector<ClipmapRing> clipmap_rings;

  void allocate(uint32_t res) {
    resolution = res;
    size_t texel_count = static_cast<size_t>(res) * res;
    heightmap.resize(texel_count, 0.0f);
    erosion_map.resize(texel_count, 0.0f);
    normal_map.resize(texel_count, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
    splat_map.resize(texel_count, glm::u8vec4(255, 0, 0, 0));
  }

  void clear() {
    heightmap.clear();
    erosion_map.clear();
    normal_map.clear();
    splat_map.clear();
    clipmap_rings.clear();
    resolution = 0;
  }

  float sample_height(float u, float v) const {
    if (heightmap.empty() || resolution == 0) return 0.0f;

    float fx = u * static_cast<float>(resolution - 1);
    float fy = v * static_cast<float>(resolution - 1);
    int x0 = static_cast<int>(fx);
    int y0 = static_cast<int>(fy);
    int x1 = std::min(x0 + 1, static_cast<int>(resolution - 1));
    int y1 = std::min(y0 + 1, static_cast<int>(resolution - 1));
    float tx = fx - static_cast<float>(x0);
    float ty = fy - static_cast<float>(y0);

    float h00 = heightmap[y0 * resolution + x0];
    float h10 = heightmap[y0 * resolution + x1];
    float h01 = heightmap[y1 * resolution + x0];
    float h11 = heightmap[y1 * resolution + x1];

    return (h00 * (1 - tx) + h10 * tx) * (1 - ty) +
           (h01 * (1 - tx) + h11 * tx) * ty;
  }
};

} // namespace astralix::terrain
