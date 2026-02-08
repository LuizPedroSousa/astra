#include "systems/render-system/passes/debug-pass.hpp"
#include "base.hpp"
#include "components/light/light-component.hpp"
#include "components/material/material-component.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "entities/object.hpp"
#include "guid.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "renderer-api.hpp"
#include "storage-buffer.hpp"
#include <GL/gl.h>
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>
namespace astralix {

DebugNormal::DebugNormal(ENTITY_INIT_PARAMS) : ENTITY_INIT() {
  add_component<ResourceComponent>();
  m_active = false;
};

void DebugNormal::start() {

};

void DebugNormal::update(Ref<RenderTarget> render_target,
                         MeshCollector &mesh_collector,
                         StorageBuffer *mesh_collector_storage,
                         MeshContext *mesh_ctx) {
  CHECK_ACTIVE(this);

  auto resource = get_component<ResourceComponent>();

  resource->update();

  auto shader = resource->shader();

  auto entity_manager = EntityManager::get();
  auto component_manager = ComponentManager::get();

  auto light_components = component_manager->get_components<LightComponent>();

  auto camera = entity_manager->get_entity_with_component<CameraComponent>()
                    ->get_component<CameraComponent>();

  struct ObjectRollback {
    uint32_t obj_index = -1;
    ResourceDescriptorID shader_id;
  };

  auto objects = entity_manager->get_entities<Object>();

  std::vector<ObjectRollback> rollback;

  rollback.reserve(objects.size());

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

    obj_resource->set_shader("debug_normal");

    obj_mesh->change_draw_type(RendererAPI::DrawPrimitive::POINTS);

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

    shader->set_matrix("view", camera->get_view_matrix());
    shader->set_vec3("view_position", obj_transform->position);
    shader->set_matrix("projection", camera->get_projection_matrix());

    shader->unbind();
  }

  mesh_collector.draw(objects, render_target->renderer_api(),
                      mesh_collector_storage, mesh_ctx);

  for (auto data : rollback) {
    if (data.obj_index != -1) {
      auto object = objects[data.obj_index];
      auto obj_resource = object->get_component<ResourceComponent>();
      auto obj_mesh = object->get_component<MeshComponent>();

      if (!has_components(obj_resource, obj_mesh))
        continue;

      obj_mesh->change_draw_type(RendererAPI::DrawPrimitive::TRIANGLES);

      obj_resource->set_shader(data.shader_id);
    }
  }
}

DebugDepth::DebugDepth(ENTITY_INIT_PARAMS) : ENTITY_INIT() {
  add_component<MeshComponent>()->attach_mesh(Mesh::quad(1.0f));
  add_component<ResourceComponent>();
  m_active = false;
};

void DebugDepth::start(Ref<RenderTarget> render_target) {
  auto resource = get_component<ResourceComponent>();
  auto mesh = get_component<MeshComponent>();

  resource->set_shader("debug_depth");

  auto shader = resource->shader();

  resource->start();
  shader->set_int("dephMap", 1);
  mesh->start(render_target);
};

void DebugDepth::update(Ref<RenderTarget> render_target,
                        Framebuffer *shadow_mapping_framebuffer) {
  CHECK_ACTIVE(this);

  if (shadow_mapping_framebuffer == nullptr) {
    return;
  }

  auto resource = get_component<ResourceComponent>();
  auto mesh = get_component<MeshComponent>();

  resource->update();

  auto shader = resource->shader();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,
                shadow_mapping_framebuffer->get_color_attachment_id());

  mesh->update(render_target);
}

} // namespace astralix
