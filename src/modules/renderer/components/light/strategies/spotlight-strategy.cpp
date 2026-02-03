#include "spotlight-strategy.hpp"
#include "components/camera/camera-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "guid.hpp"

namespace astralix {

SpotLightStrategy::SpotLightStrategy() : LightStrategy() {};

void SpotLightStrategy::update(IEntity *source, Object *object,
                               EntityID &camera_id, size_t &index) {
  auto resource = object->get_component<ResourceComponent>();

  auto camera = EntityManager::get()->get_entity<Object>(camera_id);

  auto camera_transform = camera->get_component<TransformComponent>();
  auto camera_component = camera->get_component<CameraComponent>();

  auto shader = resource->shader();

  shader->set_vec3("spot_light.position", camera_transform->position);
  shader->set_vec3("spot_light.direction", camera_component->front);
  shader->set_float("spot_light.inner_cut_off", glm::cos(glm::radians(12.5f)));
  shader->set_float("spot_light.outer_cut_off", glm::cos(glm::radians(20.5f)));
  shader->set_vec3("spot_light.exposure.ambient", glm::vec3(0.2f));
  shader->set_vec3("spot_light.exposure.diffuse", glm::vec3(0.5f));
  shader->set_vec3("spot_light.exposure.specular", glm::vec3(1.0f));
}

} // namespace astralix
