#include "debug-system.hpp"
#include "base.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "entities/object.hpp"
#include "events/key-codes.hpp"
#include "events/key-event.hpp"

#include "glad//glad.h"
#include "guid.hpp"
#include "managers/resource-manager.hpp"
#include "managers/system-manager.hpp"
#include "managers/window-manager.hpp"
#include "path.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/shader.hpp"
#include <string>

#include "systems/render-system/shadow-mapping-system.hpp"
#include "targets/render-target.hpp"

namespace astralix {

DebugSystem::DebugSystem(Ref<RenderTarget> render_target)
    : m_render_target(render_target) {
  m_entity_manager = new EntityManager();
}

DebugNormal::DebugNormal(ENTITY_INIT_PARAMS) : ENTITY_INIT() {
  add_component<ResourceComponent>();
  m_active = false;
};

void DebugNormal::start() {

};

void DebugSystem::pre_update(double dt) {}

void DebugSystem::fixed_update(double fixed_dt) {}

void DebugNormal::update() {
  CHECK_ACTIVE(this);

  auto resource = get_component<ResourceComponent>();
  auto mesh = get_component<MeshComponent>();

  resource->update();

  auto shader = resource->shader();

  auto entity_manager = EntityManager::get();
  auto component_manager = ComponentManager::get();

  entity_manager->for_each<Object>([&](Object *object) {
    auto resource = object->get_component<ResourceComponent>();
    auto mesh = object->get_component<MeshComponent>();
    auto transform = object->get_component<TransformComponent>();

    ResourceDescriptorID current_shader_id;

    if (resource != nullptr) {
      if (resource->has_shader()) {
        current_shader_id = resource->shader()->descriptor_id();

        resource->set_shader("debug_normal");
      }
    }

    if (mesh != nullptr) {
      mesh->change_draw_type(RendererAPI::DrawPrimitive::POINTS);
    }

    object->update();

    if (mesh != nullptr) {
      mesh->change_draw_type(RendererAPI::DrawPrimitive::TRIANGLES);
    }

    if (resource != nullptr) {
      if (!current_shader_id.empty()) {
        resource->set_shader(current_shader_id);
      }
    }
  });
}

DebugDepth::DebugDepth(ENTITY_INIT_PARAMS) : ENTITY_INIT() {
  add_component<MeshComponent>()->attach_mesh(Mesh::quad(1.0f));
  add_component<ResourceComponent>();
  m_active = false;
};

void DebugDepth::start(Ref<RenderTarget> render_target) {
  auto resource_manager = ResourceManager::get();

  auto resource = get_component<ResourceComponent>();
  auto mesh = get_component<MeshComponent>();

  resource->set_shader("debug_depth");

  auto shader = resource->shader();

  resource->start();
  shader->set_int("dephMap", 0);
  mesh->start(render_target);
};

void DebugDepth::update(Ref<RenderTarget> render_target) {
  CHECK_ACTIVE(this);
  auto system_manager = SystemManager::get();

  auto shadow_mapping = system_manager->get_system<ShadowMappingSystem>();

  if (shadow_mapping == nullptr || !shadow_mapping->is_active()) {
    return;
  }

  auto resource = get_component<ResourceComponent>();
  auto mesh = get_component<MeshComponent>();

  resource->update();

  auto shader = resource->shader();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,
                shadow_mapping->framebufer()->get_color_attachment_id());

  mesh->update(render_target);
}

void DebugSystem::start() {
  auto resource_manager = ResourceManager::get();

  Shader::create("debug_normal", "shaders/fragment/debug_normal.glsl"_engine,
                 "shaders/vertex/debug_normal.glsl"_engine,
                 "shaders/geometry/debug_normal.glsl"_engine);

  Shader::create("debug_depth", "shaders/fragment/debug_depth.glsl"_engine,
                 "shaders/vertex/debug_depth.glsl"_engine);

  resource_manager->load_from_descriptors_by_ids<ShaderDescriptor>(
      m_render_target->renderer_api()->get_backend(),
      {"debug_normal", "debug_depth"});

  auto event_dispatcher = EventDispatcher::get();

  m_entity_manager->add_entity<DebugNormal>("normal");
  m_entity_manager->add_entity<DebugDepth>("depth")->start(m_render_target);

  event_dispatcher->attach<input::KeyboardListener, input::KeyReleasedEvent>(
      [&](input::KeyReleasedEvent *event) {
        switch (event->key_code) {

        default:
          return;
        }
      });
}

#define GET_ENTITY(T) m_entity_manager->get_entity<T>();

void DebugSystem::update(double dt) {
  CHECK_ACTIVE(this);

  auto depth = GET_ENTITY(DebugDepth);
  auto normal = GET_ENTITY(DebugNormal);

  if (IS_KEY_RELEASED(input::KeyCode::F2)) {
    depth->set_active(!depth->is_active());
  }

  if (IS_KEY_RELEASED(input::KeyCode::F3)) {
    normal->set_active(!normal->is_active());
  }

  depth->update(m_render_target);
  normal->update();
}

DebugSystem::~DebugSystem() { delete m_entity_manager; }

} // namespace astralix
