#pragma once

#include "components/camera/camera-component.hpp"
#include "components/light/light-component.hpp"
#include "components/material/material-component.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "entities/camera.hpp"
#include "entities/object.hpp"
#include "framebuffer.hpp"
#include "managers/entity-manager.hpp"
#include "managers/resource-manager.hpp"
#include "render-pass.hpp"
#include "renderer-api.hpp"
#include "systems/render-system/collectors/mesh-collector.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include <GL/gl.h>

namespace astralix {

class ForwardPass : public RenderPass {
public:
  ForwardPass() = default;
  ~ForwardPass() override = default;

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;

    for (auto resource : resources) {
      switch (resource->desc.type) {
        case RenderGraphResourceType::Framebuffer: {
          if (resource->desc.name == "shadow_map") {
            m_shadow_map = resource->get_framebuffer();
          }

          if (resource->desc.name == "scene_color") {
            m_scene_color = resource->get_framebuffer();
          }
          break;
        }

        case RenderGraphResourceType::LogicalBuffer: {
          if (resource->desc.name == "mesh_context") {
            m_mesh_context =
                static_cast<MeshContext *>(resource->get_logical_buffer());
          }
          break;
        }

        case RenderGraphResourceType::StorageBuffer: {
          if (resource->desc.name == "mesh_collector_storage") {
            m_mesh_collector_storage = resource->get_storage_buffer();
          }
          break;
        }

        default:
          break;
      }
    }

    if (m_scene_color == nullptr || m_mesh_context == nullptr ||
        m_mesh_collector_storage == nullptr) {
      set_enabled(false);
      return;
    }

    auto entity_manager = EntityManager::get();

    entity_manager->for_each<Object>([&](Object *object) {
      auto resource = object->get_component<ResourceComponent>();
      auto mesh = object->get_component<MeshComponent>();
      auto transform = object->get_component<TransformComponent>();

      if (resource != nullptr && resource->is_active()) {
        resource->start();
      }

      if (transform != nullptr && transform->is_active()) {
        transform->start();
      }

      if (mesh != nullptr && mesh->is_active()) {
        mesh->start(render_target);
      }
    });
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    ASTRA_PROFILE_N("ForwardPass Update");

    auto entity_manager = EntityManager::get();
    auto component_manager = ComponentManager::get();

    entity_manager->for_each<Camera>([&](Camera *target) {
      target->update();

      auto camera = target->get_component<CameraComponent>();

      camera->recalculate_projection_matrix(m_scene_color);
      camera->recalculate_view_matrix();
    });

    auto camera_entity =
        entity_manager->get_entity_with_component<CameraComponent>();
    if (camera_entity == nullptr) {
      return;
    }

    auto camera = camera_entity->get_component<CameraComponent>();
    auto camera_transform = camera_entity->get_component<TransformComponent>();
    auto light_components = component_manager->get_components<LightComponent>();
    auto objects = entity_manager->get_entities<Object>();

    for (auto object : objects) {
      ASTRA_PROFILE_N("ForwardPass Object Loop");

      auto transform = object->get_component<TransformComponent>();
      auto material = object->get_component<MaterialComponent>();
      auto resource = object->get_component<ResourceComponent>();

      if (resource == nullptr || !resource->has_shader()) {
        continue;
      }

      if (transform != nullptr) {
        transform->update();
      }

      resource->update();

      if (material != nullptr) {
        material->update();
      }

      auto shader = resource->shader();

      if (m_shadow_map != nullptr) {
        const int slot = resource_manager()->texture_2d_slot();

        shader->set_int("light.shadow_map", slot);

        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_shadow_map->get_depth_attachment_id());
      }

      for (size_t i = 0; i < light_components.size(); i++) {
        light_components[i]->update(object, i);
      }

      shader->set_matrix("camera.view", camera->get_view_matrix());
      shader->set_vec3("camera.position", camera_transform->position);
      shader->set_matrix("camera.projection", camera->get_projection_matrix());
    }

    auto renderer_api = m_render_target->renderer_api();

    m_scene_color->bind();
    renderer_api->enable_buffer_testing();
    renderer_api->depth(RendererAPI::DepthMode::Less);

    m_mesh_collector.draw(objects, renderer_api, m_mesh_collector_storage,
                          m_mesh_context);

    m_scene_color->unbind();
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "ForwardPass"; }

private:
  Framebuffer *m_shadow_map = nullptr;
  Framebuffer *m_scene_color = nullptr;

  MeshCollector m_mesh_collector;
  MeshContext *m_mesh_context = nullptr;
  StorageBuffer *m_mesh_collector_storage = nullptr;
};

} // namespace astralix
