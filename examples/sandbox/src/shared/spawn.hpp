#pragma once

#include "astralix/modules/physics/components/rigidbody.hpp"
#include "astralix/modules/renderer/components/transform.hpp"
#include "astralix/modules/renderer/entities/scene.hpp"
#include <array>
#include <glm/glm.hpp>
#include <string>

using namespace astralix;

astralix::scene::Transform make_transform(glm::vec3 position, glm::vec3 scale = glm::vec3(1.0f));

void rotate_transform(astralix::scene::Transform &transform, glm::vec3 axis, float degrees);

struct cube_spawn_spec {
  const char *name;
  glm::vec3 position;
  glm::vec3 scale;
  float mass = 1.0f;
  glm::vec3 rotation_axis = glm::vec3(0.0f);
  float rotation_degrees = 0.0f;
};

extern const std::array<cube_spawn_spec, 2> ramp_specs;
extern const std::array<cube_spawn_spec, 9> dynamic_cube_specs;

void spawn_arena_cube(Scene &scene, std::string name, glm::vec3 position, glm::vec3 scale, physics::RigidBodyMode mode, float mass = 1.0f, glm::vec3 rotation_axis = glm::vec3(0.0f), float rotation_degrees = 0.0f);
