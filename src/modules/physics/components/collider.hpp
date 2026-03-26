#pragma once

#include "glm/glm.hpp"

namespace astralix::physics {

struct BoxCollider {
  glm::vec3 half_extents = glm::vec3(0.5f);
  glm::vec3 center = glm::vec3(0.0f);
};

struct FitBoxColliderFromRenderMesh {};

} // namespace astralix::physics
