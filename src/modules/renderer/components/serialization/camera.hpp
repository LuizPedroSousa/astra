#pragma once

#include "components/camera.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"
#include <string>

namespace astralix::serialization {

inline std::string
camera_controller_mode_to_string(scene::CameraControllerMode mode) {
  switch (mode) {
    case scene::CameraControllerMode::Free:
      return "free";
    case scene::CameraControllerMode::FirstPerson:
      return "first_person";
    case scene::CameraControllerMode::ThirdPerson:
      return "third_person";
    case scene::CameraControllerMode::Orbital:
      return "orbital";
  }

  return "unknown";
}

inline scene::CameraControllerMode
camera_controller_mode_from_string(const std::string &mode) {
  if (mode == "first_person") {
    return scene::CameraControllerMode::FirstPerson;
  }

  if (mode == "third_person") {
    return scene::CameraControllerMode::ThirdPerson;
  }

  if (mode == "orbital") {
    return scene::CameraControllerMode::Orbital;
  }

  return scene::CameraControllerMode::Free;
}

inline ComponentSnapshot snapshot_component(const rendering::Camera &camera) {
  ComponentSnapshot snapshot{.name = "Camera"};
  serialization::fields::append_vec3(snapshot.fields, "up", camera.up);
  serialization::fields::append_vec3(snapshot.fields, "front", camera.front);
  serialization::fields::append_vec3(snapshot.fields, "rotation",
                                     camera.rotation);
  serialization::fields::append_vec3(snapshot.fields, "direction",
                                     camera.direction);
  snapshot.fields.push_back({"fov_degrees", camera.fov_degrees});
  snapshot.fields.push_back({"near_plane", camera.near_plane});
  snapshot.fields.push_back({"far_plane", camera.far_plane});
  snapshot.fields.push_back({"orthographic_scale", camera.orthographic_scale});
  snapshot.fields.push_back({"orthographic", camera.orthographic});
  return snapshot;
}

inline ComponentSnapshot
snapshot_component(const scene::CameraController &controller) {
  ComponentSnapshot snapshot{.name = "CameraController"};
  snapshot.fields.push_back(
      {"mode", camera_controller_mode_to_string(controller.mode)});
  snapshot.fields.push_back({"yaw", controller.yaw});
  snapshot.fields.push_back({"pitch", controller.pitch});
  snapshot.fields.push_back({"speed", controller.speed});
  snapshot.fields.push_back({"sensitivity", controller.sensitivity});
  snapshot.fields.push_back({"orbit_distance", controller.orbit_distance});
  snapshot.fields.push_back(
      {"third_person_distance", controller.third_person_distance});
  serialization::fields::append_vec3(snapshot.fields, "third_person_offset",
                                     controller.third_person_offset);
  if (controller.target) {
    snapshot.fields.push_back(
        {"target", static_cast<std::string>(*controller.target)});
  }
  return snapshot;
}

inline void
apply_camera_snapshot(ecs::EntityRef entity,
                      const serialization::fields::FieldList &fields) {
  entity.emplace<rendering::Camera>(rendering::Camera{
      .up = serialization::fields::read_vec3(fields, "up",
                                             glm::vec3(0.0f, 1.0f, 0.0f)),
      .front = serialization::fields::read_vec3(fields, "front",
                                                glm::vec3(0.0f, 0.0f, 1.0f)),
      .rotation = serialization::fields::read_vec3(fields, "rotation"),
      .direction =
          serialization::fields::read_vec3(fields, "direction",
                                           glm::vec3(0.0f, 0.0f, 1.0f)),
      .view_matrix = glm::mat4(1.0f),
      .projection_matrix = glm::mat4(1.0f),
      .fov_degrees =
          serialization::fields::read_float(fields, "fov_degrees")
              .value_or(45.0f),
      .near_plane = serialization::fields::read_float(fields, "near_plane")
                        .value_or(0.1f),
      .far_plane = serialization::fields::read_float(fields, "far_plane")
                       .value_or(100.0f),
      .orthographic_scale =
          serialization::fields::read_float(fields, "orthographic_scale")
              .value_or(10.0f),
      .orthographic =
          serialization::fields::read_bool(fields, "orthographic")
              .value_or(false),
  });
}

inline void apply_camera_controller_snapshot(ecs::EntityRef entity,
                                             const serialization::fields::
                                                 FieldList &fields) {
  entity.emplace<scene::CameraController>(scene::CameraController{
      .mode = camera_controller_mode_from_string(
          serialization::fields::read_string(fields, "mode")
              .value_or("free")),
      .yaw = serialization::fields::read_float(fields, "yaw")
                 .value_or(-90.0f),
      .pitch = serialization::fields::read_float(fields, "pitch")
                   .value_or(0.0f),
      .speed = serialization::fields::read_float(fields, "speed")
                   .value_or(4.0f),
      .sensitivity = serialization::fields::read_float(fields, "sensitivity")
                         .value_or(0.1f),
      .orbit_distance =
          serialization::fields::read_float(fields, "orbit_distance")
              .value_or(5.0f),
      .third_person_distance =
          serialization::fields::read_float(fields, "third_person_distance")
              .value_or(5.0f),
      .third_person_offset =
          serialization::fields::read_vec3(fields, "third_person_offset",
                                           glm::vec3(0.0f, 2.0f, -5.0f)),
      .target = serialization::fields::read_entity_id(fields, "target"),
  });
}

} // namespace astralix::serialization
