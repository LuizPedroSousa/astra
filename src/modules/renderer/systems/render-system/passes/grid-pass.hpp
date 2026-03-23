#pragma once

#include "components/camera/camera-component.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "entities/entity.hpp"
#include "managers/entity-manager.hpp"
#include "managers/resource-manager.hpp"
#include "render-pass.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "targets/render-target.hpp"
#include <GL/gl.h>

namespace astralix {

class GridEntity : public Entity<GridEntity> {
public:
  GridEntity(ENTITY_INIT_PARAMS) : ENTITY_INIT() {
    add_component<MeshComponent>()->attach_mesh(Mesh::quad(1.0f));
    add_component<ResourceComponent>();
  }

  void on_enable() override {};
  void on_disable() override {};
};

class GridPass : public RenderPass {
public:
  GridPass() = default;
  ~GridPass() override { delete m_entity_manager; }

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;
    m_entity_manager = new EntityManager();

    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        m_render_target->renderer_api()->get_backend(), {"shaders::grid"});

    auto grid = m_entity_manager->add_entity<GridEntity>("grid");

    auto resource = grid->get_component<ResourceComponent>();
    auto mesh = grid->get_component<MeshComponent>();

    resource->set_shader("shaders::grid");
    resource->start();
    mesh->start(m_render_target);
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    static constexpr int kSurfaceRenderMode = 0;
    static constexpr int kYAxisRenderMode = 1;

    auto grid = m_entity_manager->get_entity<GridEntity>();

    if (grid == nullptr) {
      return;
    }

    auto component_manager = ComponentManager::get();
    auto camera_components =
        component_manager->get_components<CameraComponent>();

    if (camera_components.empty()) {
      return;
    }

    auto camera = camera_components[0];
    auto renderer_api = m_render_target->renderer_api();

    auto resource = grid->get_component<ResourceComponent>();
    auto mesh = grid->get_component<MeshComponent>();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    renderer_api->depth(RendererAPI::DepthMode::LessEqual);
    glDepthMask(GL_FALSE);

    resource->update();

    auto shader = resource->shader();

    shader->set_matrix("grid.view", camera->get_view_matrix());
    shader->set_matrix("grid.projection", camera->get_projection_matrix());
    shader->set_int("grid.render_mode", kSurfaceRenderMode);

    mesh->update(m_render_target);

    glDisable(GL_DEPTH_TEST);
    shader->set_int("grid.render_mode", kYAxisRenderMode);
    mesh->update(m_render_target);
    glEnable(GL_DEPTH_TEST);

    glDepthMask(GL_TRUE);
    renderer_api->depth(RendererAPI::DepthMode::Less);
    glDisable(GL_BLEND);
  }

  void end(double dt) override {}

  void cleanup() override { delete m_entity_manager; }

  std::string name() const override { return "GridPass"; }

private:
  EntityManager *m_entity_manager = nullptr;
};

} // namespace astralix
