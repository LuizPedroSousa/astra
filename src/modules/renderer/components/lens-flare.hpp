#pragma once

namespace astralix::rendering {

struct LensFlare {
  bool enabled = true;
  float intensity = 1.0f;
  float threshold = 0.8f;
  int ghost_count = 4;
  float ghost_dispersal = 0.35f;
  float ghost_weight = 0.5f;
  float halo_radius = 0.6f;
  float halo_weight = 0.25f;
  float halo_thickness = 0.1f;
  float chromatic_aberration = 0.02f;
};

} // namespace astralix::rendering
