#include "camera.hpp"
#include "base.hpp"
#include "components/camera/camera-component.hpp"
#include "components/transform/transform-component.hpp"
#include "entities/ientity.hpp"
#include "events/key-codes.hpp"
#include "events/keyboard.hpp"
#include "events/mouse.hpp"
#include "glm/geometric.hpp"
#include "log.hpp"
#include "managers/window-manager.hpp"
#include "time.hpp"

namespace astralix {

Camera::Camera(ENTITY_INIT_PARAMS, CameraMode mode, glm::vec3 position)
    : ENTITY_INIT(), m_mode(mode) {
  add_component<TransformComponent>(position);
  add_component<CameraComponent>();
}

void Camera::update() {
  CHECK_ACTIVE(this);

  auto entity_manager = EntityManager::get();
  auto component_manager = ComponentManager::get();

  auto transform = get_component<TransformComponent>();

  auto camera = component_manager->get_components<CameraComponent>()[0];

  if (transform == nullptr || !transform->is_active()) {
    return;
  }

  using namespace astralix::input;

  switch (m_mode) {
  case CameraMode::Free: {
    if (IS_KEY_DOWN(KeyCode::W)) {
      transform->position +=
          camera->direction * m_speed * Time::get()->get_deltatime();
    }

    if (IS_KEY_DOWN(KeyCode::S)) {
      transform->position -=
          camera->direction * m_speed * Time::get()->get_deltatime();
    }

    if (IS_KEY_DOWN(KeyCode::A)) {
      transform->position -=
          glm::normalize(glm::cross(camera->front, camera->up)) * m_speed *
          Time::get()->get_deltatime();
    }

    if (IS_KEY_DOWN(KeyCode::D)) {
      transform->position +=
          glm::normalize(glm::cross(camera->front, camera->up)) * m_speed *
          Time::get()->get_deltatime();
    }

    if (IS_KEY_DOWN(KeyCode::Space)) {
      transform->position.y += m_speed * Time::get()->get_deltatime();
    }

    if (IS_KEY_DOWN(KeyCode::LeftControl)) {
      transform->position.y -= m_speed * Time::get()->get_deltatime();
    }

    auto mouse = MOUSE_DELTA();

    if (mouse.x != 0 || mouse.y != 0) {
      m_yaw += mouse.x * m_sensitivity;
      m_pitch -= mouse.y * m_sensitivity;

      if (m_pitch > 89.0f)
        m_pitch = 89.0f;
      if (m_pitch < -89.0f)
        m_pitch = -89.0f;

      camera->direction.x =
          cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
      camera->direction.y = sin(glm::radians(m_pitch));
      camera->direction.z =
          sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

      camera->front = glm::normalize(camera->direction);
    }
    break;
  }

  case CameraMode::FirstPerson: {
    if (IS_KEY_DOWN(KeyCode::W)) {
      glm::vec3 forward = camera->direction;
      forward.y = 0.0f;
      forward = glm::normalize(forward);
      transform->position += forward * m_speed * Time::get()->get_deltatime();
    }

    if (IS_KEY_DOWN(KeyCode::S)) {
      glm::vec3 forward = camera->direction;
      forward.y = 0.0f;
      forward = glm::normalize(forward);
      transform->position -= forward * m_speed * Time::get()->get_deltatime();
    }

    if (IS_KEY_DOWN(KeyCode::A)) {
      glm::vec3 right = glm::normalize(glm::cross(camera->front, camera->up));
      right.y = 0.0f;
      right = glm::normalize(right);
      transform->position -= right * m_speed * Time::get()->get_deltatime();
    }

    if (IS_KEY_DOWN(KeyCode::D)) {
      glm::vec3 right = glm::normalize(glm::cross(camera->front, camera->up));
      right.y = 0.0f;
      right = glm::normalize(right);
      transform->position += right * m_speed * Time::get()->get_deltatime();
    }

    auto mouse = MOUSE_DELTA();

    if (mouse.x != 0 || mouse.y != 0) {
      m_yaw += mouse.x * m_sensitivity;
      m_pitch -= mouse.y * m_sensitivity;

      if (m_pitch > 89.0f)
        m_pitch = 89.0f;
      if (m_pitch < -89.0f)
        m_pitch = -89.0f;

      camera->direction.x =
          cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
      camera->direction.y = sin(glm::radians(m_pitch));
      camera->direction.z =
          sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

      camera->front = glm::normalize(camera->direction);
    }
    break;
  }

  case CameraMode::ThirdPerson: {
    if (target == nullptr || !target->is_active()) {
      LOG_WARN("ThirdPerson camera mode requires a target");
      break;
    }

    glm::vec3 target_pos = target->position;
    glm::vec3 desired_pos = target_pos + m_third_person_offset;

    float smoothness = 5.0f * Time::get()->get_deltatime();
    transform->position =
        glm::mix(transform->position, desired_pos, smoothness);

    camera->direction = glm::normalize(target_pos - transform->position);
    camera->front = camera->direction;

    auto mouse = MOUSE_DELTA();
    if (mouse.x != 0) {
      float angle = mouse.x * m_sensitivity * 0.1f;
      glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(angle),
                                       glm::vec3(0.0f, 1.0f, 0.0f));
      m_third_person_offset =
          glm::vec3(rotation * glm::vec4(m_third_person_offset, 1.0f));
    }

    if (IS_KEY_DOWN(KeyCode::Equal)) {
      m_third_person_distance -= 0.5f * Time::get()->get_deltatime() * 10.0f;
      m_third_person_distance = glm::max(m_third_person_distance, 1.0f);
      float current_distance = glm::length(m_third_person_offset);
      if (current_distance > 0.0f) {
        m_third_person_offset =
            glm::normalize(m_third_person_offset) * m_third_person_distance;
      }
    }

    if (IS_KEY_DOWN(KeyCode::Minus)) {
      m_third_person_distance += 0.5f * Time::get()->get_deltatime() * 10.0f;
      m_third_person_distance = glm::min(m_third_person_distance, 20.0f);
      float current_distance = glm::length(m_third_person_offset);
      if (current_distance > 0.0f) {
        m_third_person_offset =
            glm::normalize(m_third_person_offset) * m_third_person_distance;
      }
    }
    break;
  }

  case CameraMode::Orbital: {
    if (target == nullptr || !target->is_active()) {
      LOG_WARN("Orbital camera mode requires a target");
      break;
    }

    glm::vec3 target_pos = target->position;

    auto mouse = MOUSE_DELTA();

    if (mouse.x != 0 || mouse.y != 0) {
      m_yaw += mouse.x * m_sensitivity;
      m_pitch -= mouse.y * m_sensitivity;

      if (m_pitch > 89.0f)
        m_pitch = 89.0f;
      if (m_pitch < -89.0f)
        m_pitch = -89.0f;
    }

    if (IS_KEY_DOWN(KeyCode::Equal)) {
      m_orbit_distance -= 0.5f * Time::get()->get_deltatime() * 10.0f;
      m_orbit_distance = glm::max(m_orbit_distance, 1.0f);
    }

    if (IS_KEY_DOWN(KeyCode::Minus)) {
      m_orbit_distance += 0.5f * Time::get()->get_deltatime() * 10.0f;
      m_orbit_distance = glm::min(m_orbit_distance, 50.0f);
    }

    float cam_x = target_pos.x + m_orbit_distance * cos(glm::radians(m_pitch)) *
                                     cos(glm::radians(m_yaw));
    float cam_y = target_pos.y + m_orbit_distance * sin(glm::radians(m_pitch));
    float cam_z = target_pos.z + m_orbit_distance * cos(glm::radians(m_pitch)) *
                                     sin(glm::radians(m_yaw));

    transform->position = glm::vec3(cam_x, cam_y, cam_z);

    camera->direction = glm::normalize(target_pos - transform->position);
    camera->front = camera->direction;
    break;
  }
  }

  transform->update();
}

} // namespace astralix
