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
#include "log.hpp"
#include "managers/entity-manager.hpp"
#include "managers/resource-manager.hpp"
#include "render-pass.hpp"
#include "renderer-api.hpp"
#include "systems/render-system/collectors/mesh-collector.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix {

class GeometryPass : public RenderPass {
public:
  GeometryPass() = default;
  ~GeometryPass() override = default;

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;
    for (auto resource : resources) {

      switch (resource->desc.type) {
        case RenderGraphResourceType::Framebuffer: {
          if (resource->desc.name == "scene_color") {
            m_scene_color = resource->get_framebuffer();
          }

          if (resource->desc.name == "g_buffer") {
            m_g_buffer = resource->get_framebuffer();
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

    if (m_scene_color == nullptr || m_g_buffer == nullptr ||
        m_mesh_context == nullptr || m_mesh_collector_storage == nullptr) {
      set_enabled(false);
      return;
    }

    auto entity_manager = EntityManager::get();

    entity_manager->for_each<Object>([&](Object *object) {
      auto resource = object->get_component<ResourceComponent>();
      auto mesh = object->get_component<MeshComponent>();
      auto transform = object->get_component<TransformComponent>();

      if (resource != nullptr && resource->is_active())
        resource->start();

      if (transform != nullptr && transform->is_active())
        transform->start();

      if (mesh != nullptr && mesh->is_active())
        mesh->start(render_target);
    });
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    ASTRA_PROFILE_N("GeometryPass Update");

    auto entity_manager = EntityManager::get();
    auto component_manager = ComponentManager::get();

    entity_manager->for_each<Camera>([&](Camera *target) {
      target->update();

      auto camera = target->get_component<CameraComponent>();

      camera->recalculate_projection_matrix(m_scene_color);
      camera->recalculate_view_matrix();
    });

    auto light_components = component_manager->get_components<LightComponent>();

    auto camera_entity =
        entity_manager->get_entity_with_component<CameraComponent>();
    auto camera = camera_entity->get_component<CameraComponent>();
    auto camera_transform = camera_entity->get_component<TransformComponent>();

    struct ObjectRollback {
      uint32_t obj_index = -1;
      ResourceDescriptorID shader_id;
    };

    auto objects = entity_manager->get_entities<Object>();

    std::vector<ObjectRollback> rollback;
    rollback.reserve(objects.size());

    m_g_buffer->bind();
    m_render_target->renderer_api()->clear_buffers(ClearBufferType::Color |
                                                   ClearBufferType::Depth);

    for (int32_t obj_index = 0; obj_index < objects.size(); obj_index++) {
      auto object = objects[obj_index];

      auto obj_resource = object->get_component<ResourceComponent>();
      auto obj_mesh = object->get_component<MeshComponent>();
      auto obj_transform = object->get_component<TransformComponent>();
      auto obj_material = object->get_component<MaterialComponent>();

      if (!has_components(obj_resource, obj_mesh, obj_transform))
        continue;

      rollback.emplace_back(ObjectRollback{
          .obj_index = static_cast<uint32_t>(obj_index),
          .shader_id = obj_resource->shader_descriptor_id(),
      });

      obj_resource->set_shader("shaders::g_buffer");

      obj_transform->update();

      obj_resource->update();

      if (obj_material != nullptr) {
        obj_material->update();
      }

      for (size_t i = 0; i < light_components.size(); i++) {
        light_components[i]->update(object, i);
      }

      auto shader = obj_resource->shader();
      shader->bind();

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
      {
        using namespace shader_bindings::engine_shaders_g_buffer_axsl;

        CameraParams camera_params;
        camera_params.view = camera->get_view_matrix();
        camera_params.position = camera_transform->position;
        camera_params.projection = camera->get_projection_matrix();

        shader->set_all(camera_params);
      }
#endif

      shader->unbind();
    }

    m_mesh_collector.draw(objects, m_render_target->renderer_api(),
                          m_mesh_collector_storage, m_mesh_context);

    m_g_buffer->bind(FramebufferBindType::Read);
    m_scene_color->bind(FramebufferBindType::Draw);

    const FramebufferSpecification &spec = m_g_buffer->get_specification();
    m_scene_color->blit(spec.width, spec.height, FramebufferBlitType::Depth);

    for (auto data : rollback) {
      if (data.obj_index != -1) {
        auto object = objects[data.obj_index];
        auto obj_resource = object->get_component<ResourceComponent>();
        auto obj_mesh = object->get_component<MeshComponent>();

        if (!has_components(obj_resource, obj_mesh))
          continue;

        obj_resource->set_shader(data.shader_id);
      }
    }

    m_g_buffer->unbind();
    m_scene_color->unbind();
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const noexcept override { return "GeometryPass"; }

private:
  Framebuffer *m_scene_color = nullptr;
  Framebuffer *m_g_buffer = nullptr;

  MeshCollector m_mesh_collector;
  MeshContext *m_mesh_context = nullptr;
  StorageBuffer *m_mesh_collector_storage = nullptr;
};

} // namespace astralix
