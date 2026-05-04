#pragma once

#include "glm/glm.hpp"
#include "guid.hpp"
#include <cstdint>
#include <optional>

namespace astralix::rendering {

enum class LightType : uint8_t {
  Directional = 0,
  Point = 1,
  Spot = 2,
};

struct Light {
  LightType type = LightType::Directional;
  glm::vec3 color = glm::vec3(1.0f);
  float intensity = 1.0f;
  float ambient_strength = 0.2f;
  float diffuse_strength = 0.5f;
  float specular_strength = 0.5f;
  bool casts_shadows = true;
};

struct PointLightAttenuation {
  float constant = 1.0f;
  float linear = 0.045f;
  float quadratic = 0.0075f;
};

struct SpotLightCone {
  float inner_cutoff_cos = 0.976296f;
  float outer_cutoff_cos = 0.953717f;
};

struct SpotLightAttenuation {
  float constant = 1.0f;
  float linear = 0.045f;
  float quadratic = 0.0075f;
};

struct DirectionalShadowSettings {
  float ortho_extent = 10.0f;
  float near_plane = 1.0f;
  float far_plane = 100.0f;
  float shadow_intensity = 1.0f;
};

struct SpotLightTarget {
  std::optional<EntityID> camera;
};

} // namespace astralix::rendering
