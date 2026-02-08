#pragma once

#include "components/mesh/mesh-component.hpp"
#include "components/post-processing/post-processing-component.hpp"
#include "components/resource/resource-component.hpp"
#include "entities/post-processing.hpp"
#include "managers/entity-manager.hpp"
#include "managers/resource-manager.hpp"
#include "render-pass.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"

namespace astralix {

class PostProcessPass : public RenderPass {
public:
  PostProcessPass() = default;
  ~PostProcessPass() override = default;

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;

    Shader::create("shaders::hdr", "shaders/fragment/hdr.glsl"_engine,
                   "shaders/vertex/postprocessing.glsl"_engine);

    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        m_render_target->renderer_api()->get_backend(), {"shaders::hdr"});

    EntityManager::get()->add_entity<PostProcessing>("HDR", "shaders::hdr");

    auto entity_manager = EntityManager::get();
    entity_manager->for_each<PostProcessing>(
        [&](PostProcessing *post_processing) {
          post_processing->add_component<MeshComponent>()->attach_mesh(
              Mesh::quad(1.0f));
          post_processing->add_component<ResourceComponent>()->attach_shader(
              post_processing->shader_descriptor_id);

          auto resource = post_processing->get_component<ResourceComponent>();
          auto mesh = post_processing->get_component<MeshComponent>();
          auto post_processing_comp =
              post_processing->get_component<PostProcessingComponent>();

          resource->start();

          post_processing_comp->start(render_target);
          mesh->start(render_target);
        });
  }

  void begin(double dt) override { m_render_target->unbind(); }

  void execute(double dt) override {
    auto entity_manager = EntityManager::get();

    auto entities = entity_manager->get_entities<PostProcessing>();

    for (auto entity : entities) {
      m_render_target->framebuffer()->bind(FramebufferBindType::Default, 0);
      m_render_target->renderer_api()->disable_buffer_testing();
      m_render_target->renderer_api()->clear_color();
      m_render_target->renderer_api()->clear_buffers();

      auto resource = entity->get_component<ResourceComponent>();
      auto post_processing = entity->get_component<PostProcessingComponent>();

      auto mesh = entity->get_component<MeshComponent>();

      resource->update();
      post_processing->post_update(m_render_target);

      mesh->update(m_render_target);
    }
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "PostProcessPass"; }
};

} // namespace astralix
