#pragma once

#include "components/camera/camera-component.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "entities/skybox.hpp"
#include "managers/entity-manager.hpp"
#include "render-pass.hpp"
#include "renderer-api.hpp"
#include <GL/gl.h>

namespace astralix {

class SkyboxPass : public RenderPass {
public:
  SkyboxPass() = default;
  ~SkyboxPass() override = default;

  void setup(Ref<RenderTarget> render_target, const std::vector<const RenderGraphResource*>& resources) override {
    m_render_target = render_target;

    auto entity_manager = EntityManager::get();
    entity_manager->for_each<Skybox>([&](Skybox *skybox) {
      auto resource = skybox->get_component<ResourceComponent>();
      auto mesh = skybox->get_component<MeshComponent>();

      resource->attach_cubemap({skybox->cubemap_id, "_skybox"});
      resource->set_shader(skybox->shader_id);

      resource->start();
      mesh->attach_mesh(Mesh::cube(2.0f));
      mesh->start(render_target);
    });
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    auto entity_manager = EntityManager::get();
    auto renderer_api = m_render_target->renderer_api();
    entity_manager->for_each<Skybox>([&](Skybox *skybox) {
      CHECK_ACTIVE(skybox);
      renderer_api->depth(RendererAPI::DepthMode::LessEqual);
      auto entity_manager = EntityManager::get();

      // if (!entity_manager->has_entity_with_component<CameraComponent>()) {
      //   return;
      // }

      auto resource = skybox->get_component<ResourceComponent>();
      auto mesh = skybox->get_component<MeshComponent>();

      auto component_manager = ComponentManager::get();

      auto camera = component_manager->get_components<CameraComponent>()[0];

      resource->update();

      if (camera != nullptr) {
        auto shader = resource->shader();

        auto view_without_transformation =
            glm::mat4(glm::mat3(camera->get_view_matrix()));
        shader->set_matrix("view_without_transformation",
                           view_without_transformation);
        shader->set_matrix("projection", camera->get_projection_matrix());
      }

      mesh->update(m_render_target);

      renderer_api->depth(RendererAPI::DepthMode::Less);
    });
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "SkyboxPass"; }
};

} // namespace astralix
