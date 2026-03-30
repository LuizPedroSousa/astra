#include "spawn.hpp"

#include "astralix/modules/renderer/components/transform.hpp"
#include "glm/gtx/quaternion.hpp"

using namespace astralix;

namespace {

const glm::vec3 ramp_rotation_axis(0.0f, 0.0f, 1.0f);

glm::vec3 normalize_or_default(glm::vec3 axis, glm::vec3 fallback = glm::vec3(0.0f, 1.0f, 0.0f)) {
  if (glm::length(axis) == 0.0f) {
    return fallback;
  }

  return glm::normalize(axis);
}

} // namespace

const std::array<cube_spawn_spec, 2> ramp_specs{{
    {
        .name = "arena_ramp_left",
        .position = glm::vec3(-3.75f, 0.9f, -1.5f),
        .scale = glm::vec3(4.0f, 0.5f, 2.5f),
        .mass = 1.0f,
        .rotation_axis = ramp_rotation_axis,
        .rotation_degrees = -25.0f,
    },
    {
        .name = "arena_ramp_right",
        .position = glm::vec3(3.5f, 0.9f, 2.5f),
        .scale = glm::vec3(3.5f, 0.5f, 2.0f),
        .mass = 1.0f,
        .rotation_axis = ramp_rotation_axis,
        .rotation_degrees = 22.0f,
    },
}};

const std::array<cube_spawn_spec, 9> dynamic_cube_specs{{
    {
        .name = "drop_cube_left_0",
        .position = glm::vec3(-4.75f, 4.5f, -1.5f),
        .scale = glm::vec3(0.8f),
        .mass = 1.0f,
    },
    {
        .name = "drop_cube_left_1",
        .position = glm::vec3(-4.5f, 6.0f, -1.0f),
        .scale = glm::vec3(0.8f),
        .mass = 1.0f,
    },
    {
        .name = "drop_cube_left_2",
        .position = glm::vec3(-4.0f, 7.5f, -1.8f),
        .scale = glm::vec3(0.8f),
        .mass = 1.2f,
    },
    {
        .name = "stack_cube_center_0",
        .position = glm::vec3(0.0f, 1.0f, 0.0f),
        .scale = glm::vec3(0.9f),
        .mass = 1.0f,
    },
    {
        .name = "stack_cube_center_1",
        .position = glm::vec3(0.0f, 2.25f, 0.0f),
        .scale = glm::vec3(0.9f),
        .mass = 1.0f,
    },
    {
        .name = "stack_cube_center_2",
        .position = glm::vec3(0.0f, 3.5f, 0.0f),
        .scale = glm::vec3(0.9f),
        .mass = 1.0f,
    },
    {
        .name = "stack_cube_center_3",
        .position = glm::vec3(1.1f, 1.0f, 0.0f),
        .scale = glm::vec3(0.9f),
        .mass = 1.0f,
    },
    {
        .name = "stack_cube_center_4",
        .position = glm::vec3(1.1f, 2.25f, 0.0f),
        .scale = glm::vec3(0.9f),
        .mass = 1.0f,
    },
    {
        .name = "drop_cube_right_heavy",
        .position = glm::vec3(3.75f, 6.5f, 2.5f),
        .scale = glm::vec3(1.15f),
        .mass = 2.5f,
    },
}};

astralix::scene::Transform make_transform(glm::vec3 position, glm::vec3 scale) {
  return astralix::scene::Transform{
      .position = position,
      .scale = scale,
      .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
      .matrix = glm::mat4(1.0f),
      .dirty = true,
  };
}

void rotate_transform(astralix::scene::Transform &transform, glm::vec3 axis, float degrees) {
  const glm::vec3 normalized_axis = normalize_or_default(axis);
  const glm::quat delta =
      glm::angleAxis(glm::radians(degrees), normalized_axis);

  transform.rotation = glm::normalize(delta * transform.rotation);
  transform.dirty = true;
}
