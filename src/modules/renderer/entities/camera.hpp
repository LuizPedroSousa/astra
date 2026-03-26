#pragma once
#include "components/transform/transform-component.hpp"
#include "entities/entity.hpp"
#include "entities/ientity.hpp"
#include "entities/object.hpp"
#include "glm/glm.hpp"

namespace astralix {

enum CameraMode { Free = 0, FirstPerson = 1, ThirdPerson = 2, Orbital = 3 };

class Camera : public Entity<Camera> {

public:
  Camera(ENTITY_INIT_PARAMS, CameraMode mode,
         glm::vec3 position = glm::vec3(0.0f));

  void update();

  void on_enable() override {}
  void on_disable() override {}

  void set_target(TransformComponent *target_transform) {
    target = target_transform;
  }
  void set_speed(float speed) { m_speed = speed; }
  void set_sensitivity(float sensitivity) { m_sensitivity = sensitivity; }
  void set_orbit_distance(float distance) { m_orbit_distance = distance; }
  void set_third_person_offset(const glm::vec3 &offset) {
    m_third_person_offset = offset;
  }

private:
  CameraMode m_mode;

  float m_yaw = -90.0f;
  float m_pitch = 0.0f;

  float m_speed = 4.0f;
  float m_sensitivity = 0.1f;

  float m_orbit_distance = 5.0f;
  float m_third_person_distance = 5.0f;
  glm::vec3 m_third_person_offset = glm::vec3(0.0f, 2.0f, -5.0f);

  TransformComponent *target = nullptr;
};

} // namespace astralix
