#include "erosion-pass.hpp"
#include "trace.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <utility>
#include <vector>

namespace astralix::terrain {

void ErosionPass::process(HeightmapFrame &frame, const TerrainRecipeData &recipe) {
  ASTRA_PROFILE_N("ErosionPass::process");

  const auto &erosion = recipe.erosion;
  const uint32_t resolution = frame.resolution;

  if (resolution == 0 || frame.heightmap.empty())
    return;

  std::mt19937 rng(recipe.noise.seed + 1);
  std::uniform_real_distribution<float> distribution(0.0f, static_cast<float>(resolution - 1));

  std::vector<std::pair<int, int>> brush_offsets;
  std::vector<float> brush_weights;
  float weight_sum = 0.0f;

  int radius = static_cast<int>(erosion.erode_radius);
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
      if (distance <= static_cast<float>(radius)) {
        float weight = std::max(0.0f, static_cast<float>(radius) - distance);
        brush_offsets.emplace_back(dx, dy);
        brush_weights.push_back(weight);
        weight_sum += weight;
      }
    }
  }
  for (auto &weight : brush_weights)
    weight /= weight_sum;

  auto gradient_at = [&](float px, float py) -> glm::vec2 {
    int x0 = std::clamp(static_cast<int>(px), 0, static_cast<int>(resolution) - 2);
    int y0 = std::clamp(static_cast<int>(py), 0, static_cast<int>(resolution) - 2);
    float u = px - static_cast<float>(x0);
    float v = py - static_cast<float>(y0);

    float h00 = frame.heightmap[y0 * resolution + x0];
    float h10 = frame.heightmap[y0 * resolution + x0 + 1];
    float h01 = frame.heightmap[(y0 + 1) * resolution + x0];
    float h11 = frame.heightmap[(y0 + 1) * resolution + x0 + 1];

    float gx = (h10 - h00) * (1 - v) + (h11 - h01) * v;
    float gy = (h01 - h00) * (1 - u) + (h11 - h10) * u;
    return {gx, gy};
  };

  for (uint32_t iteration = 0; iteration < erosion.iterations; ++iteration) {
    float px = distribution(rng);
    float py = distribution(rng);
    float dx = 0.0f, dy = 0.0f;
    float speed = 1.0f;
    float water = 1.0f;
    float sediment = 0.0f;

    for (uint32_t step = 0; step < erosion.drop_lifetime; ++step) {
      int node_x = static_cast<int>(px);
      int node_y = static_cast<int>(py);

      if (node_x < 0 || node_x >= static_cast<int>(resolution) - 1 ||
          node_y < 0 || node_y >= static_cast<int>(resolution) - 1)
        break;

      glm::vec2 gradient = gradient_at(px, py);

      dx = dx * erosion.inertia - gradient.x * (1.0f - erosion.inertia);
      dy = dy * erosion.inertia - gradient.y * (1.0f - erosion.inertia);

      float length = std::sqrt(dx * dx + dy * dy);
      if (length < 1e-6f)
        break;
      dx /= length;
      dy /= length;

      float new_px = px + dx;
      float new_py = py + dy;

      if (new_px < 0 || new_px >= static_cast<float>(resolution) - 1 ||
          new_py < 0 || new_py >= static_cast<float>(resolution) - 1)
        break;

      float old_height = frame.sample_height(px / static_cast<float>(resolution - 1), py / static_cast<float>(resolution - 1));
      float new_height = frame.sample_height(new_px / static_cast<float>(resolution - 1), new_py / static_cast<float>(resolution - 1));
      float height_diff = new_height - old_height;

      float capacity = std::max(-height_diff * speed * water * erosion.sediment_capacity, erosion.min_sediment_capacity);

      if (sediment > capacity || height_diff > 0) {
        float deposit = (height_diff > 0)
                            ? std::min(height_diff, sediment)
                            : (sediment - capacity) * erosion.deposit_speed;

        sediment -= deposit;

        float frac_x = px - static_cast<float>(node_x);
        float frac_y = py - static_cast<float>(node_y);
        int x1 = std::min(node_x + 1, static_cast<int>(resolution) - 1);
        int y1 = std::min(node_y + 1, static_cast<int>(resolution) - 1);
        float w00 = (1.0f - frac_x) * (1.0f - frac_y);
        float w10 = frac_x * (1.0f - frac_y);
        float w01 = (1.0f - frac_x) * frac_y;
        float w11 = frac_x * frac_y;

        int i00 = node_y * static_cast<int>(resolution) + node_x;
        int i10 = node_y * static_cast<int>(resolution) + x1;
        int i01 = y1 * static_cast<int>(resolution) + node_x;
        int i11 = y1 * static_cast<int>(resolution) + x1;

        frame.heightmap[i00] = std::clamp(frame.heightmap[i00] + deposit * w00, 0.0f, 1.0f);
        frame.heightmap[i10] = std::clamp(frame.heightmap[i10] + deposit * w10, 0.0f, 1.0f);
        frame.heightmap[i01] = std::clamp(frame.heightmap[i01] + deposit * w01, 0.0f, 1.0f);
        frame.heightmap[i11] = std::clamp(frame.heightmap[i11] + deposit * w11, 0.0f, 1.0f);
        frame.erosion_map[i00] += deposit * w00;
        frame.erosion_map[i10] += deposit * w10;
        frame.erosion_map[i01] += deposit * w01;
        frame.erosion_map[i11] += deposit * w11;
      } else {
        float erode_amount = std::min((capacity - sediment) * erosion.erode_speed, -height_diff);

        for (size_t brush_index = 0; brush_index < brush_offsets.size(); ++brush_index) {
          int bx = node_x + brush_offsets[brush_index].first;
          int by = node_y + brush_offsets[brush_index].second;
          if (bx >= 0 && bx < static_cast<int>(resolution) &&
              by >= 0 && by < static_cast<int>(resolution)) {
            int bidx = by * static_cast<int>(resolution) + bx;
            float delta = erode_amount * brush_weights[brush_index];
            frame.heightmap[bidx] = std::clamp(frame.heightmap[bidx] - delta, 0.0f, 1.0f);
            frame.erosion_map[bidx] -= delta;
          }
        }
        sediment += erode_amount;
      }

      speed = std::sqrt(std::max(speed * speed + height_diff * erosion.gravity, 0.0f));
      water *= (1.0f - erosion.evaporate_speed);
      px = new_px;
      py = new_py;
    }
  }

  float min_height = *std::min_element(frame.heightmap.begin(), frame.heightmap.end());
  float max_height = *std::max_element(frame.heightmap.begin(), frame.heightmap.end());
  float range = max_height - min_height;
  if (range > 0.0f) {
    for (auto &height : frame.heightmap) {
      height = (height - min_height) / range;
    }
  }
}

} // namespace astralix::terrain
