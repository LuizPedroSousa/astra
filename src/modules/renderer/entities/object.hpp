#pragma once
#include "entities/entity.hpp"
#include "entities/ientity.hpp"
#include "targets/render-target.hpp"

namespace astralix {

struct ObjectDTO {
  ENTITY_PARAMS
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
};

class Object : public Entity<Object> {
public:
  Object(ENTITY_INIT_PARAMS, glm::vec3 position = glm::vec3(0.0f),
         glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f));

  Object() {};

  void start(Ref<RenderTarget> render_target);

  virtual void update() {};
  virtual void fixed_update(float dt) {};

  void on_enable() override {};
  void on_disable() override {};
};
} // namespace astralix
