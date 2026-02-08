#pragma once

#include "components/camera/camera-component.hpp"
#include "components/light/light-component.hpp"
#include "components/material/material-component.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "entities/camera.hpp"
#include "entities/object.hpp"
#include "managers/entity-manager.hpp"
#include "managers/resource-manager.hpp"
#include "render-pass.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include <GL/gl.h>

namespace astralix {

class GeometryPass : public RenderPass {
public:
  GeometryPass() = default;
  ~GeometryPass() override = default;

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    for (auto resource : resources) {
      if (resource->desc.type == RenderGraphResourceType::Framebuffer) {
        if (resource->desc.name == "shadow_map") {
          m_shadow_map_framebuffer = resource->get_framebuffer();
          continue;
        }

        if (resource->desc.name == "scene_color") {
          m_scene_color_framebuffer = resource->get_framebuffer();
        }
      }
    }

    if (m_scene_color_framebuffer == nullptr)
      return;

    auto entity_manager = EntityManager::get();

    entity_manager->for_each<Object>([&](Object *object) {
      auto resource = object->get_component<ResourceComponent>();
      auto mesh = object->get_component<MeshComponent>();
      auto transform = object->get_component<TransformComponent>();

      if (resource != nullptr && resource->is_active())
        resource->start();

      auto shader = resource->shader();

      if (m_shadow_map_framebuffer != nullptr && shader != nullptr) {
        shader->set_int("shadowMap", 1);
      }

      if (transform != nullptr && transform->is_active())
        transform->start();

      if (mesh != nullptr && mesh->is_active())
        mesh->start(render_target);
    });
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    ASTRA_PROFILE_N("LightSystem Update");

    auto entity_manager = EntityManager::get();
    auto component_manager = ComponentManager::get();

    entity_manager->for_each<Camera>([&](Camera *target) {
      target->update();

      auto camera = target->get_component<CameraComponent>();

      camera->recalculate_projection_matrix(m_scene_color_framebuffer);
      camera->recalculate_view_matrix();
    });

    auto light_components = component_manager->get_components<LightComponent>();

    auto camera = entity_manager->get_entity_with_component<CameraComponent>()
                      ->get_component<CameraComponent>();

    entity_manager->for_each<Object>([&](Object *object) {
      ASTRA_PROFILE_N("GeometryPass Object Loop");

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

      if (m_shadow_map_framebuffer != nullptr) {
        auto resource = object->get_component<ResourceComponent>();

        if (resource == nullptr || !resource->has_shader()) {
          return;
        }

        auto shader = resource->shader();

        int slot = resource_manager()->texture_2d_slot();

        shader->set_int("shadowMap", slot);

        glActiveTexture(GL_TEXTURE0 + slot);

        glBindTexture(GL_TEXTURE_2D,
                      m_shadow_map_framebuffer->get_color_attachment_id());
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

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const noexcept override { return "GeometryPass"; }

private:
  Framebuffer *m_shadow_map_framebuffer = nullptr;
  Framebuffer *m_scene_color_framebuffer = nullptr;
};

} // namespace astralix
