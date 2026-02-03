#include "render-system.hpp"
#include "components/post-processing/post-processing-component.hpp"
#include "entities/ientity.hpp"
#include "events/event-scheduler.hpp"
#include "glad/glad.h"
#include "managers/entity-manager.hpp"

#include "debug-system.hpp"
#include "engine.hpp"
#include "entities/object.hpp"
#include "entities/post-processing.hpp"
#include "entities/skybox.hpp"
#include "entities/text.hpp"
#include "managers/resource-manager.hpp"
#include "project.hpp"
#include "resources/descriptors/font-descriptor.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "systems/render-system/hdr-system.hpp"
#include "systems/render-system/light-system.hpp"
#include "systems/render-system/mesh-system.hpp"
#include "systems/render-system/shadow-mapping-system.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"

namespace astralix {
RenderSystem::RenderSystem(RenderSystemConfig &config)
    : m_config(config) {

      };

void RenderSystem::start() {
  m_render_target = RenderTarget::create(m_config.backend_to_api(),
                                         m_config.msaa_to_render_target_msaa(),
                                         m_config.window_id);

  auto entity_manager = EntityManager::get();

  m_render_target->init();

  resource_manager()
      ->load_from_descriptors<ShaderDescriptor, Texture2DDescriptor,
                              Texture3DDescriptor, MaterialDescriptor,
                              FontDescriptor>(
          m_render_target->renderer_api()->get_backend());

  EntityManager::get()->for_each<Skybox>(
      [&](Skybox *skybox) { skybox->start(m_render_target); });

  entity_manager->for_each<Object>(
      [&](Object *object) { object->start(m_render_target); });

  entity_manager->for_each<Text>(
      [&](Text *text) { text->start(m_render_target); });

  add_subsystem<HDRSystem>(m_render_target)->start();

  entity_manager->for_each<PostProcessing>(
      [&](PostProcessing *post_processing) {
        post_processing->start(m_render_target);
      });

  add_subsystem<ShadowMappingSystem>(m_render_target)->start();
  add_subsystem<DebugSystem>(m_render_target)->start();
  add_subsystem<LightSystem>(m_render_target)->start();
  add_subsystem<MeshSystem>(m_render_target)->start();
}

void RenderSystem::fixed_update(double fixed_dt) {
  ASTRA_PROFILE_N("RenderSystem FixedUpdate");

  auto entity_manager = EntityManager::get();

  entity_manager->for_each<Object>(
      [&](Object *object) { object->fixed_update(fixed_dt); });
};

void RenderSystem::pre_update(double dt) {
  ASTRA_PROFILE_N("RenderSystem PreUpdate");

  auto engine = Engine::get();

  auto entity_manager = EntityManager::get();

  auto post_processings = entity_manager->get_entities<PostProcessing>();

  bool has_post_processing = false;

  for (auto post_processing : post_processings) {
    if (!post_processing->is_active()) {
      continue;
    }

    auto comp = post_processing->get_component<PostProcessingComponent>();

    if (comp != nullptr && comp->is_active()) {
      has_post_processing = true;
      break;
    }
  }

  m_render_target->bind(has_post_processing);

  EntityManager::get()->for_each<Skybox>(
      [&](Skybox *skybox) { skybox->pre_update(); });
};

void RenderSystem::update(double dt) {
  ASTRA_PROFILE_N("RenderSystem Update");

  auto entity_manager = EntityManager::get();

  auto shadow_mapping = get_subsystem<ShadowMappingSystem>();
  auto debug = get_subsystem<DebugSystem>();
  auto mesh = get_subsystem<MeshSystem>();
  auto light = get_subsystem<LightSystem>();
  auto hdr = get_subsystem<HDRSystem>();

  if (shadow_mapping != nullptr) {
    shadow_mapping->update(dt);
  }

  EntityManager::get()->for_each<Skybox>(
      [&](Skybox *skybox) { skybox->update(m_render_target); });

  light->update(dt);

  entity_manager->for_each<Text>([&](Text *text) { text->update(); });

  mesh->update(dt);

  auto scheduler = EventScheduler::get();

  if (debug != nullptr)
    debug->update(dt);

  m_render_target->unbind();

  if (hdr != nullptr)
    hdr->update(dt);

  entity_manager->for_each<Skybox>(
      [&](Skybox *skybox) { skybox->post_update(); });

  entity_manager->for_each<PostProcessing>(
      [&](PostProcessing *post_processing) {
        post_processing->post_update(m_render_target);
      });

  scheduler->bind(SchedulerType::REALTIME);
};

RenderSystem::~RenderSystem() {}

} // namespace astralix
