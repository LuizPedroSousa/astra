#pragma once

#include "glm/glm.hpp"
#include "guid.hpp"
#include <cstdint>
#include <optional>

namespace astralix::scene {

enum class CameraControllerMode : uint8_t {
  Free = 0,
  FirstPerson = 1,
  ThirdPerson = 2,
  Orbital = 3,
};

struct CameraController {
  CameraControllerMode mode = CameraControllerMode::Free;
  float yaw = -90.0f;
  float pitch = 0.0f;
  float speed = 4.0f;
  float sensitivity = 0.05f;
  float orbit_distance = 5.0f;
  float third_person_distance = 5.0f;
  glm::vec3 third_person_offset = glm::vec3(0.0f, 2.0f, -5.0f);
  std::optional<EntityID> target;
};

} // namespace astralix::scene

namespace astralix::rendering {

struct Camera {
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 front = glm::vec3(0.0f, 0.0f, 1.0f);
  glm::vec3 rotation = glm::vec3(0.0f);
  glm::vec3 direction = glm::vec3(0.0f, 0.0f, 1.0f);

  glm::mat4 view_matrix = glm::mat4(1.0f);
  glm::mat4 projection_matrix = glm::mat4(1.0f);

  float fov_degrees = 45.0f;
  float near_plane = 0.1f;
  float far_plane = 100.0f;
  float orthographic_scale = 10.0f;
  bool orthographic = false;
  std::optional<float> runtime_near_plane_override;
  std::optional<float> runtime_far_plane_override;
};

} // namespace astralix::rendering
