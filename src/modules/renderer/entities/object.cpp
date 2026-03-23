#include "object.hpp"
#include "base.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "entities/ientity.hpp"
#include <string>

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix {

Object::Object(ENTITY_INIT_PARAMS, glm::vec3 position, glm::vec3 scale)
    : ENTITY_INIT() {
  add_component<ResourceComponent>();
  add_component<TransformComponent>(position, scale);
}

void Object::start(Ref<RenderTarget> render_target) {
  CHECK_ACTIVE(this);
#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
  using namespace shader_bindings::engine_shaders_light_axsl;
#endif

  auto resource = get_component<ResourceComponent>();
  auto mesh = get_component<MeshComponent>();
  auto transform = get_component<TransformComponent>();

  if (resource != nullptr && resource->is_active())
    resource->start();

  auto shader = resource->shader();

  if (shader != nullptr) {
#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    shader->set(LightUniform::shadow_map, 1);
#else
    shader->set_int("light.shadow_map", 1);
#endif
  }

  if (transform != nullptr && transform->is_active())
    transform->start();

  if (mesh != nullptr && mesh->is_active())
    mesh->start(render_target);
}

} // namespace astralix
