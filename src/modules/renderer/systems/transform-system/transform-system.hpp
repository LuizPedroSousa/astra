#pragma once

#include "components/transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"
#include "world.hpp"

namespace astralix::scene {

inline void recalculate_transform(scene::Transform &transform) {
  if (!transform.dirty) {
    return;
  }

  if (glm::length(transform.rotation) == 0.0f) {
    transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  }

  const glm::mat4 translation =
      glm::translate(glm::mat4(1.0f), transform.position);
  const glm::mat4 rotation = glm::toMat4(transform.rotation);
  const glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform.scale);

  transform.matrix = translation * rotation * scale;
  transform.dirty = false;
}

inline void update_transforms(ecs::World &world) {
  world.each<scene::Transform>(
      [](EntityID, scene::Transform &transform) { recalculate_transform(transform); });
}

} // namespace astralix::scene
