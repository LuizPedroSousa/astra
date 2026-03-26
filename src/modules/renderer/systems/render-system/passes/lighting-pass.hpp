#pragma once

#include "components/camera/camera-component.hpp"
#include "components/light/light-component.hpp"
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
#include "shaders/engine_shaders_g_buffer_axsl.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include <GL/gl.h>

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix {

class LightingPass : public RenderPass {
public:
  LightingPass() = default;
  ~LightingPass() override { delete m_entity_manager; }

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

          if (resource->desc.name == "g_buffer") {
            m_g_buffer = resource->get_framebuffer();
          }
          break;
        }

        default:
          break;
      }
    }

    if (m_scene_color == nullptr || m_g_buffer == nullptr) {
      set_enabled(false);
      return;
    }

    m_entity_manager = new EntityManager();

    auto quad = m_entity_manager->add_entity<Object>("lighting_quad");
    quad->add_component<MeshComponent>()->attach_mesh(Mesh::quad(1.0f));
    quad->add_component<ResourceComponent>();

    auto quad_resource = quad->get_component<ResourceComponent>();
    auto quad_mesh = quad->get_component<MeshComponent>();

    quad_resource->set_shader("shaders::lighting");
    quad_resource->start();
    quad_mesh->start(m_render_target);
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    ASTRA_PROFILE_N("LightingPass Update");

    auto quad = m_entity_manager->get_entity<Object>();

    if (quad == nullptr) {
      return;
    }

    m_scene_color->bind();

    auto component_manager = ComponentManager::get();
    auto global_entity_manager = EntityManager::get();

    auto light_components = component_manager->get_components<LightComponent>();

    auto camera_entity =
        global_entity_manager->get_entity_with_component<CameraComponent>();
    auto camera = camera_entity->get_component<CameraComponent>();
    auto camera_transform = camera_entity->get_component<TransformComponent>();

    auto quad_resource = quad->get_component<ResourceComponent>();
    auto quad_mesh = quad->get_component<MeshComponent>();

    quad_resource->update();

    auto shader = quad_resource->shader();
    shader->bind();

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    using namespace shader_bindings::engine_shaders_light_axsl;
#endif

    int32_t texture_unit = 0;

    if (m_shadow_map != nullptr) {
      glActiveTexture(GL_TEXTURE0 + texture_unit);
      glBindTexture(GL_TEXTURE_2D, m_shadow_map->get_depth_attachment_id());
#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
      shader->set(LightUniform::shadow_map, texture_unit);
#endif
      texture_unit++;
    }

    auto color_attachments = m_g_buffer->get_color_attachments();
    for (uint32_t i = 0; i < color_attachments.size(); i++) {
      glActiveTexture(GL_TEXTURE0 + texture_unit + i);
      glBindTexture(GL_TEXTURE_2D, color_attachments[i]);
    }

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    shader->set(LightUniform::g_position, texture_unit);
    shader->set(LightUniform::g_normal, texture_unit + 1);
    shader->set(LightUniform::g_albedo, texture_unit + 2);
#endif

    for (size_t i = 0; i < light_components.size(); i++) {
      light_components[i]->update(quad, i);
    }

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    {
      shader->set(
          shader_bindings::engine_shaders_light_axsl::CameraUniform::position,
          camera_transform->position);
    }
#endif

    glDisable(GL_DEPTH_TEST);

    quad_mesh->update(m_render_target);

    glEnable(GL_DEPTH_TEST);

    shader->unbind();

    m_scene_color->unbind();
  }

  void end(double dt) override {}

  void cleanup() override { delete m_entity_manager; }

  std::string name() const noexcept override { return "LightingPass"; }

private:
  EntityManager *m_entity_manager = nullptr;

  Framebuffer *m_shadow_map = nullptr;
  Framebuffer *m_scene_color = nullptr;
  Framebuffer *m_g_buffer = nullptr;
};

} // namespace astralix
