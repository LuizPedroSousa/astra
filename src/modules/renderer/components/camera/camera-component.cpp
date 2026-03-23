#include "camera-component.hpp"
#include "components/transform/transform-component.hpp"

#include "components/camera/serializers/camera-component-serializer.hpp"
#include "event-dispatcher.hpp"
#include "framebuffer.hpp"

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix {

CameraComponent::CameraComponent(COMPONENT_INIT_PARAMS)
    : COMPONENT_INIT(CameraComponent, "camera", true,
                     create_ref<CameraComponentSerializer>(this)),
      m_is_orthographic(false) {

  auto event_dispatcher = EventDispatcher::get();
}

void CameraComponent::use_orthographic() { m_is_orthographic = true; }

void CameraComponent::use_perspective() { m_is_orthographic = false; }

void CameraComponent::recalculate_projection_matrix(
    Framebuffer *target_framebuffer) {
  if (m_is_orthographic) {
    m_projection_matrix = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f);
  } else {
    const auto &spec = target_framebuffer->get_specification();

    m_projection_matrix = glm::perspective(
        45.0f, (float)spec.width / (float)spec.height, 0.1f, 100.0f);
  }
}

void CameraComponent::recalculate_view_matrix() {
  auto matrix = glm::mat4(1.0f);

  auto transform = get_owner()->get_component<TransformComponent>();

  matrix = glm::lookAt(transform->position, transform->position + front, up);

  m_view_matrix = matrix;
}

void CameraComponent::update(Ref<Shader> &shader,
                             Framebuffer *target_framebuffer) {
  recalculate_view_matrix();
  recalculate_projection_matrix(target_framebuffer);

  auto transform = get_owner()->get_component<TransformComponent>();

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
  using namespace shader_bindings::engine_shaders_light_axsl;

  shader->set_all(CameraParams{.view = m_view_matrix,
                               .projection = m_projection_matrix,
                               .position = transform->position});
#else
  shader->set_matrix("camera.view", m_view_matrix);
  shader->set_matrix("camera.projection", m_projection_matrix);
  shader->set_vec3("camera.position", transform->position);
#endif
}

} // namespace astralix
