#pragma once

#include "systems/transform-system/transform-system.hpp"
#include "components/camera.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "world.hpp"
#include <algorithm>
#include <cmath>

namespace astralix::scene {

struct CameraControllerInput {
  bool forward = false;
  bool backward = false;
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;
  glm::vec2 mouse_delta = glm::vec2(0.0f);
  float dt = 0.0f;
  float aspect_ratio = 1.0f;
};

inline void recalculate_camera_view_matrix(rendering::Camera &camera,
                                           const Transform &transform,
                                           float aspect_ratio) {
  const float clamped_aspect = std::max(aspect_ratio, 0.001f);

  camera.view_matrix = glm::lookAt(
      transform.position, transform.position + camera.front, camera.up);
}

inline void recalculate_camera_projection_matrix(rendering::Camera &camera,
                                                 const Transform &transform,
                                                 float aspect_ratio) {
  const float clamped_aspect = std::max(aspect_ratio, 0.001f);

  camera.projection_matrix =
      glm::perspective(glm::radians(camera.fov_degrees), clamped_aspect,
                       camera.near_plane, camera.far_plane);
}

inline void recalculate_camera_orthographic_matrix(rendering::Camera &camera,
                                                   const Transform &transform,
                                                   float aspect_ratio) {
  const float clamped_aspect = std::max(aspect_ratio, 0.001f);

  const float half_width = camera.orthographic_scale * clamped_aspect;
  const float half_height = camera.orthographic_scale;
  camera.projection_matrix =
      glm::ortho(-half_width, half_width, -half_height, half_height,
                 camera.near_plane, camera.far_plane);
}

inline void update_camera_controller(ecs::World &world, EntityID entity_id,
                                     Transform &transform, rendering::Camera &camera,
                                     CameraController &controller,
                                     const CameraControllerInput &input) {
  controller.yaw += input.mouse_delta.x * controller.sensitivity;
  controller.pitch -= input.mouse_delta.y * controller.sensitivity;
  controller.pitch = std::clamp(controller.pitch, -89.0f, 89.0f);

  camera.direction.x =
      cos(glm::radians(controller.yaw)) * cos(glm::radians(controller.pitch));
  camera.direction.y = sin(glm::radians(controller.pitch));
  camera.direction.z =
      sin(glm::radians(controller.yaw)) * cos(glm::radians(controller.pitch));
  camera.front = glm::normalize(camera.direction);

  const glm::vec3 strafe = glm::normalize(glm::cross(camera.front, camera.up));
  const float velocity = controller.speed * input.dt;

  const bool has_target = controller.target.has_value() &&
                          world.contains(*controller.target) &&
                          world.has<Transform>(*controller.target);

  switch (controller.mode) {
    case CameraControllerMode::Free:
    case CameraControllerMode::FirstPerson: {
      float forward = static_cast<float>(input.forward) -
                      static_cast<float>(input.backward);
      float right =
          static_cast<float>(input.right) - static_cast<float>(input.left);
      float up = static_cast<float>(input.up) - static_cast<float>(input.down);

      transform.position += camera.direction * (forward * velocity);
      transform.position += strafe * (right * velocity);
      transform.position.y += up * velocity;

      transform.dirty = true;
      break;
    }

    case CameraControllerMode::ThirdPerson: {
      if (!has_target) {
        break;
      }

      const auto *target = world.get<Transform>(*controller.target);
      transform.position = target->position -
                           camera.front * controller.third_person_distance +
                           controller.third_person_offset;
      transform.dirty = true;
      break;
    }

    case CameraControllerMode::Orbital: {
      if (!has_target) {
        break;
      }

      const auto *target = world.get<Transform>(*controller.target);
      const float yaw = glm::radians(controller.yaw);
      const float pitch = glm::radians(controller.pitch);

      transform.position =
          target->position +
          glm::vec3(cos(yaw) * cos(pitch), sin(pitch), sin(yaw) * cos(pitch)) *
              controller.orbit_distance;
      camera.front = glm::normalize(target->position - transform.position);
      transform.dirty = true;
      break;
    }
  }

  recalculate_transform(transform);
  // recalculate_camera_matrices(camera, transform, input.aspect_ratio);

  scene::recalculate_camera_view_matrix(camera, transform,
                                            input.aspect_ratio);

  if (camera.orthographic) {
    scene::recalculate_camera_orthographic_matrix(camera, transform,
                                                      input.aspect_ratio);
  } else {
    scene::recalculate_camera_projection_matrix(camera, transform,
                                                    input.aspect_ratio);
  }
}

inline void update_camera_controllers(ecs::World &world,
                                      const CameraControllerInput &input) {
  world.each<Transform, rendering::Camera, CameraController>(
      [&](EntityID entity_id, Transform &transform, rendering::Camera &camera,
          CameraController &controller) {
        update_camera_controller(world, entity_id, transform, camera,
                                 controller, input);
      });
}

} // namespace astralix::scene
