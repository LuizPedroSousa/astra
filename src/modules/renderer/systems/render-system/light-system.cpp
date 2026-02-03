#include "light-system.hpp"

#include "base.hpp"
#include "components/camera/camera-component.hpp"
#include "components/light/light-component.hpp"
#include "components/material/material-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "entities/camera.hpp"
#include "glad//glad.h"
#include "managers/component-manager.hpp"
#include "managers/entity-manager.hpp"
#include "systems/render-system/shadow-mapping-system.hpp"
#include "trace.hpp"

namespace astralix {

LightSystem::LightSystem(Ref<RenderTarget> render_target)
    : m_render_target(render_target) {}

void LightSystem::pre_update(double dt) {}

void LightSystem::fixed_update(double fixed_dt) {}

void LightSystem::start() {}

void LightSystem::update(double dt) {
  CHECK_ACTIVE(this);
  ASTRA_PROFILE_N("LightSystem Update");

  auto entity_manager = EntityManager::get();
  auto component_manager = ComponentManager::get();

  auto objects = entity_manager->get_entities<Object>();

  entity_manager->for_each<Camera>([&](Camera *target) {
    target->update();

    auto camera = target->get_component<CameraComponent>();

    camera->recalculate_projection_matrix(m_render_target);
    camera->recalculate_view_matrix();
  });

  auto system_manager = SystemManager::get();

  auto shadow_mapping = system_manager->get_system<ShadowMappingSystem>();

  auto light_components = component_manager->get_components<LightComponent>();

  auto camera = entity_manager->get_entity_with_component<CameraComponent>()
                    ->get_component<CameraComponent>();

  entity_manager->for_each<Object>([&](Object *object) {
    ASTRA_PROFILE_N("LightSystem Object Loop");

    auto transform = object->get_component<TransformComponent>();
    auto material = object->get_component<MaterialComponent>();
    auto resource = object->get_component<ResourceComponent>();

    if (!resource->has_shader()) {
      return;
    }

    if (transform != nullptr) {
      transform->update();
    }

    resource->update();

    if (material != nullptr) {
      material->update();
    }

    if (shadow_mapping != nullptr) {
      shadow_mapping->bind_depth(object);
    }

    for (size_t i = 0; i < light_components.size(); i++) {
      light_components[i]->update(object, i);
    }

    auto shader = resource->shader();

    shader->set_matrix("view", camera->get_view_matrix());
    shader->set_vec3("view_position", transform->position);
    shader->set_matrix("projection", camera->get_projection_matrix());
  });
}

LightSystem::~LightSystem() {}

} // namespace astralix
