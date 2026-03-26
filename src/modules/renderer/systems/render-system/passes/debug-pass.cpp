#include "systems/render-system/passes/debug-pass.hpp"
#include "base.hpp"
#include "components/light/light-component.hpp"
#include "components/material/material-component.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "entities/object.hpp"
#include "events/key-codes.hpp"
#include "guid.hpp"
#include "log.hpp"
#include "managers/window-manager.hpp"
#include "renderer-api.hpp"
#include "storage-buffer.hpp"
#include <GL/gl.h>
#include <glm/gtc/quaternion.hpp>

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

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

    shader->set_matrix("entity.view", camera->get_view_matrix());
    shader->set_vec3("view_position", obj_transform->position);
    shader->set_matrix("entity.projection", camera->get_projection_matrix());

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

  resource->set_shader("shaders::debug_depth");

  auto shader = resource->shader();

  resource->start();
#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
  using namespace shader_bindings::engine_shaders_debug_depth_axsl;
  shader->set(DepthUniform::depth_map, 0);
  shader->set(DepthUniform::near_plane, 1.0f);
  shader->set(DepthUniform::far_plane, 24.0f);
#else
  shader->set_int("depth.depth_map", 0);
  shader->set_float("depth.near_plane", 1.0f);
  shader->set_float("depth.far_plane", 24.0f);
#endif
  mesh->start(render_target);
};

void DebugDepth::update(Ref<RenderTarget> render_target,
                        Framebuffer *shadow_map) {
  CHECK_ACTIVE(this);

  if (shadow_map == nullptr) {
    return;
  }

  auto resource = get_component<ResourceComponent>();
  auto mesh = get_component<MeshComponent>();

  resource->update();

  auto shader = resource->shader();

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
  using namespace shader_bindings::engine_shaders_debug_depth_axsl;
  shader->set_int("depth.fullscreen", m_fullscreen ? 1 : 0);
#else
  shader->set_int("depth.fullscreen", m_fullscreen ? 1 : 0);
#endif

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, shadow_map->get_depth_attachment_id());

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  mesh->update(render_target);
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
}

DebugGBuffer::DebugGBuffer(ENTITY_INIT_PARAMS) : ENTITY_INIT() {
  add_component<MeshComponent>()->attach_mesh(Mesh::quad(1.0f));
  add_component<ResourceComponent>();
  m_active = false;
  m_attachment_index = 0;
};

void DebugGBuffer::start(Ref<RenderTarget> render_target) {
  auto resource = get_component<ResourceComponent>();
  auto mesh = get_component<MeshComponent>();

  resource->set_shader("shaders::debug_g_buffer");
  resource->start();
  mesh->start(render_target);
};

void DebugGBuffer::update(Ref<RenderTarget> render_target,
                          Framebuffer *g_buffer) {
  CHECK_ACTIVE(this);

  if (g_buffer == nullptr) {
    return;
  }

  auto color_attachments = g_buffer->get_color_attachments();

  if (color_attachments.empty()) {
    return;
  }

  auto resource = get_component<ResourceComponent>();
  auto mesh = get_component<MeshComponent>();

  resource->update();

  auto shader = resource->shader();

  int requested_attachment_index = m_attachment_index;

  if (input::IS_KEY_RELEASED(input::KeyCode::D1)) {
    requested_attachment_index = 0;
  } else if (input::IS_KEY_RELEASED(input::KeyCode::D2)) {
    requested_attachment_index = 1;
  } else if (input::IS_KEY_RELEASED(input::KeyCode::D3)) {
    requested_attachment_index = 2;
  } else if (input::IS_KEY_RELEASED(input::KeyCode::D4)) {
    requested_attachment_index = 3;
  }

  if (requested_attachment_index >= 0 &&
      requested_attachment_index < static_cast<int>(color_attachments.size())) {
    m_attachment_index = requested_attachment_index;
  } else if (requested_attachment_index != m_attachment_index) {
    LOG_WARN("Ignoring invalid G-buffer attachment index ",
             requested_attachment_index,
             ". Available color attachments: ", color_attachments.size());
  }

  if (m_attachment_index < 0 ||
      m_attachment_index >= static_cast<int>(color_attachments.size())) {
    m_attachment_index = 0;
  }

  glActiveTexture(GL_TEXTURE0 + m_attachment_index);
  glBindTexture(GL_TEXTURE_2D, color_attachments[m_attachment_index]);
  glActiveTexture(GL_TEXTURE1 + m_attachment_index + 1);
  glBindTexture(GL_TEXTURE_2D, color_attachments[1]);

  shader->bind();

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
  {
    using namespace shader_bindings::engine_shaders_debug_g_buffer_axsl;

    shader->set(GBufferUniform::attachment, m_attachment_index);
    shader->set(GBufferUniform::g_normal_mask, 1);
    shader->set(GBufferUniform::near_plane, 0.1f);
    shader->set(GBufferUniform::far_plane, 100.0f);
  }
#endif

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  mesh->update(render_target);
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);

  shader->unbind();
}

} // namespace astralix
