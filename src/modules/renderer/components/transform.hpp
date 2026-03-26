#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

namespace astralix::scene {

struct Transform {
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 scale = glm::vec3(1.0f);
  glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  glm::mat4 matrix = glm::mat4(1.0f);
  bool dirty = true;
};

} // namespace astralix::scene
