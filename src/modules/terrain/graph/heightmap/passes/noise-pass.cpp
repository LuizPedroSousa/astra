#include "noise-pass.hpp"
#include "trace.hpp"
#include <array>
#include <cmath>
#include <numeric>
#include <random>

namespace astralix::terrain {

namespace {

struct PerlinState {
  std::array<int, 512> perm;

  void seed(uint32_t seed_value) {
    std::array<int, 256> permutation;
    std::iota(permutation.begin(), permutation.end(), 0);
    std::mt19937 rng(seed_value);
    std::shuffle(permutation.begin(), permutation.end(), rng);
    for (int index = 0; index < 256; ++index) {
      perm[index] = perm[index + 256] = permutation[index];
    }
  }

  float noise2d(float x, float y) const {
    int xi = static_cast<int>(std::floor(x)) & 255;
    int yi = static_cast<int>(std::floor(y)) & 255;
    float xf = x - std::floor(x);
    float yf = y - std::floor(y);
    float u = xf * xf * xf * (xf * (xf * 6.0f - 15.0f) + 10.0f);
    float v = yf * yf * yf * (yf * (yf * 6.0f - 15.0f) + 10.0f);

    int aa = perm[perm[xi] + yi];
    int ab = perm[perm[xi] + yi + 1];
    int ba = perm[perm[xi + 1] + yi];
    int bb = perm[perm[xi + 1] + yi + 1];

    auto grad = [](int hash, float gx, float gy) -> float {
      switch (hash & 3) {
        case 0: return  gx + gy;
        case 1: return -gx + gy;
        case 2: return  gx - gy;
        case 3: return -gx - gy;
      }
      return 0.0f;
    };

    float x1 = std::lerp(grad(aa, xf, yf), grad(ba, xf - 1.0f, yf), u);
    float x2 = std::lerp(grad(ab, xf, yf - 1.0f), grad(bb, xf - 1.0f, yf - 1.0f), u);
    return std::lerp(x1, x2, v);
  }
};

} // namespace

void NoisePass::process(HeightmapFrame &frame, const TerrainRecipeData &recipe) {
  ASTRA_PROFILE_N("NoisePass::process");

  const auto &noise = recipe.noise;

  frame.allocate(recipe.resolution);

  PerlinState perlin;
  perlin.seed(noise.seed);

  float max_value = 0.0f;
  float min_value = 0.0f;

  for (uint32_t y = 0; y < frame.resolution; ++y) {
    for (uint32_t x = 0; x < frame.resolution; ++x) {
      float amplitude = noise.amplitude;
      float frequency = noise.frequency;
      float value = 0.0f;

      for (uint32_t octave = 0; octave < noise.octaves; ++octave) {
        float sample_x = static_cast<float>(x) * frequency;
        float sample_y = static_cast<float>(y) * frequency;
        value += perlin.noise2d(sample_x, sample_y) * amplitude;
        frequency *= noise.lacunarity;
        amplitude *= noise.persistence;
      }

      frame.heightmap[y * frame.resolution + x] = value;
      max_value = std::max(max_value, value);
      min_value = std::min(min_value, value);
    }
  }

  float range = max_value - min_value;
  if (range > 0.0f) {
    for (auto &height : frame.heightmap) {
      height = (height - min_value) / range;
    }
  }
}

} // namespace astralix::terrain
