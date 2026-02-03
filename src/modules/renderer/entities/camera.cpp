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

      // limit rotation of y axis
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
    //
    break;
  }

    // case CameraMode::Orbital: {
    //   break;
    // }
  }

  transform->update();
}

} // namespace astralix
