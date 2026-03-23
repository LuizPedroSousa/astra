#include "point-strategy.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "guid.hpp"
#include "log.hpp"

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix {

inline std::string point_light_field(size_t index, std::string_view field) {
  return "light.point_lights[" + std::to_string(index) + "]." +
         std::string(field);
}

void PointStrategy::update(IEntity *source, Object *object, EntityID &camera_id,
                           size_t &index) {
  (void)camera_id;

  if (index >= 4) {
    return;
  }

  auto resource = object->get_component<ResourceComponent>();
  auto transform = source->get_component<TransformComponent>();

  auto shader = resource->shader();

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
  using namespace shader_bindings::engine_shaders_light_axsl;

  shader->set(LightUniform::point_lights__attenuation__constant, index,
              m_point.attenuation.constant);
  shader->set(LightUniform::point_lights__attenuation__linear, index,
              m_point.attenuation.linear);
  shader->set(LightUniform::point_lights__attenuation__quadratic, index,
              m_point.attenuation.quadratic);

  shader->set(LightUniform::point_lights__exposure__ambient, index,
              m_point.exposure.ambient);
  shader->set(LightUniform::point_lights__exposure__diffuse, index,
              m_point.exposure.diffuse);
  shader->set(LightUniform::point_lights__exposure__specular, index,
              m_point.exposure.specular);

  shader->set(LightUniform::point_lights__position, index, transform->position);
#else
  shader->set_float(point_light_field(index, "attenuation.constant"),
                    m_point.attenuation.constant);
  shader->set_float(point_light_field(index, "attenuation.linear"),
                    m_point.attenuation.linear);
  shader->set_float(point_light_field(index, "attenuation.quadratic"),
                    m_point.attenuation.quadratic);

  shader->set_vec3(point_light_field(index, "exposure.ambient"),
                   m_point.exposure.ambient);

  shader->set_vec3(point_light_field(index, "exposure.diffuse"),
                   m_point.exposure.diffuse);
  shader->set_vec3(point_light_field(index, "exposure.specular"),
                   m_point.exposure.specular);

  shader->set_vec3(point_light_field(index, "position"),
                   transform->position);
#endif
}

} // namespace astralix
