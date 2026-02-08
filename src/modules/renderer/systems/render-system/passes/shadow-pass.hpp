#pragma once

#include "components/light/light-component.hpp"
#include "components/material/material-component.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "framebuffer.hpp"
#include "managers/entity-manager.hpp"
#include "managers/resource-manager.hpp"
#include "render-pass.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include <GL/gl.h>

namespace astralix {

class ShadowPass : public RenderPass {
public:
  ShadowPass() = default;
  ~ShadowPass() override = default;

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;

    for (auto resource : resources) {
      if (resource->desc.type == RenderGraphResourceType::Framebuffer &&
          resource->desc.name == "shadow_map") {
        m_shadow_mapping_framebuffer = resource->get_framebuffer();
        break;
      }
    }

    if (m_shadow_mapping_framebuffer == nullptr) {
      return;
    }

    Shader::create("shadow_mapping_depth",
                   "shaders/fragment/shadow_mapping_depth.glsl"_engine,
                   "shaders/vertex/shadow_mapping_depth.glsl"_engine);

    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        m_render_target->renderer_api()->get_backend(),
        {"shadow_mapping_depth"});
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    if (m_shadow_mapping_framebuffer == nullptr) {
      return;
    }

    auto entity_manager = EntityManager::get();

    m_shadow_mapping_framebuffer->bind();

    glEnable(GL_DEPTH_TEST);

    auto renderer_api = m_render_target->renderer_api();

    renderer_api->enable_buffer_testing();
    renderer_api->clear_buffers();
    renderer_api->clear_color();

    auto component_manager = ComponentManager::get();
    auto light_components = component_manager->get_components<LightComponent>();

    entity_manager->for_each<Object>([&](Object *object) {
      renderer_api->cull_face(RendererAPI::CullFaceMode::Front);
      auto resource = object->get_component<ResourceComponent>();

      auto mesh = object->get_component<MeshComponent>();
      auto transform = object->get_component<TransformComponent>();
      auto material = object->get_component<MaterialComponent>();

      if (resource == nullptr || !resource->has_shader()) {
        return;
      }

      auto older_shader_id = resource->shader()->descriptor_id();

      resource->set_shader("shadow_mapping_depth");

      resource->update();

      if (transform != nullptr) {
        transform->update();
      }

      if (material != nullptr) {
        material->update();
      }

      for (size_t i = 0; i < light_components.size(); i++) {
        light_components[i]->update(object, i);
      }

      auto shader = resource->shader();

      shader->set_matrix("g_model", transform->matrix);

      if (mesh != nullptr) {
        mesh->update(m_render_target);
      }

      resource->set_shader(older_shader_id);

      renderer_api->cull_face(RendererAPI::CullFaceMode::Back);
    });

    m_shadow_mapping_framebuffer->unbind();
    m_render_target->framebuffer()->bind();
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "ShadowPass"; }

private:
  Framebuffer *m_shadow_mapping_framebuffer = nullptr;
};

} // namespace astralix
