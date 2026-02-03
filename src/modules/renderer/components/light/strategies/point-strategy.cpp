#include "point-strategy.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "guid.hpp"
#include "log.hpp"

namespace astralix {

inline std::string get_name(const char *name, const char *prefix, int count) {
  std::string result =
      std::string(name + std::string("[") + std::to_string(count) +
                  std::string("]") + "." + prefix);

  return result;
}

void PointStrategy::update(IEntity *source, Object *object, EntityID &camera_id,
                           size_t &index) {
  auto resource = object->get_component<ResourceComponent>();
  auto transform = source->get_component<TransformComponent>();

  auto shader = resource->shader();

  shader->set_int("light_type", 1);
  shader->set_float(get_name("point_lights", "attenuation.constant", index),
                    m_point.attenuation.constant);
  shader->set_float(get_name("point_lights", "attenuation.linear", index),
                    m_point.attenuation.linear);
  shader->set_float(get_name("point_lights", "attenuation.quadratic", index),
                    m_point.attenuation.quadratic);

  shader->set_vec3(get_name("point_lights", "exposure.ambient", index),
                   m_point.exposure.ambient);

  shader->set_vec3(get_name("point_lights", "exposure.diffuse", index),
                   m_point.exposure.diffuse);
  shader->set_vec3(get_name("point_lights", "exposure.specular", index),
                   m_point.exposure.specular);

  shader->set_vec3(get_name("point_lights", "position", index),
                   transform->position);
}

} // namespace astralix
