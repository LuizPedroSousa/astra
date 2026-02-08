#pragma once
#include "components/component.hpp"
#include "framebuffer.hpp"
#include "resources/shader.hpp"

namespace astralix {

class CameraComponent : public Component<CameraComponent> {
public:
  CameraComponent(COMPONENT_INIT_PARAMS);

  void update(Ref<Shader> &shader, Framebuffer *terget_framebuffer);

  void use_perspective();
  void use_orthographic();

  glm::mat4 get_view_matrix() { return m_view_matrix; }
  glm::mat4 get_projection_matrix() { return m_projection_matrix; }

  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 front = glm::vec3(0.003012, -0.509046, 0.860734);
  glm::vec3 rotation;

  glm::vec3 direction;

  void recalculate_projection_matrix(Framebuffer *terget_framebuffer);
  void recalculate_view_matrix();

private:
  glm::mat4 m_view_matrix;
  glm::mat4 m_projection_matrix;

  bool m_is_orthographic;
};

} // namespace astralix
