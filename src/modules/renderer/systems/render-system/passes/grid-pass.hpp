#pragma once

#include "events/key-codes.hpp"
#include "framebuffer.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "managers/window-manager.hpp"
#include "render-pass.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/mesh.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/scene-selection.hpp"
#include "trace.hpp"

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix {

class GridPass : public RenderPass {
public:
  GridPass() = default;
  ~GridPass() override = default;

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;

    for (auto resource : resources) {
      if (resource->desc.type == RenderGraphResourceType::Framebuffer &&
          resource->desc.name == "scene_color") {
        m_scene_color = resource->get_framebuffer();
      }
    }

    if (m_scene_color == nullptr) {
      set_enabled(false);
      LOG_WARN("[GridPass] Skipping setup: scene_color framebuffer is not "
               "available");
      return;
    }

    const auto backend = m_render_target->renderer_api()->get_backend();
    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        backend, {"shaders::grid"});
    m_shader =
        resource_manager()->get_by_descriptor_id<Shader>("shaders::grid");

    rendering::ensure_mesh_uploaded(m_grid_quad, m_render_target);
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    ASTRA_PROFILE_N("GridPass");
    static constexpr int k_surface_render_mode = 0;
    static constexpr int k_y_axis_render_mode = 1;

    if (input::IS_KEY_RELEASED(input::KeyCode::F1)) {
      m_active = !m_active;
    }

    if (!m_active) {
      return;
    }

    if (m_scene_color == nullptr) {
      LOG_WARN("[GridPass] Skipping execute: scene_color framebuffer is not "
               "available");
      return;
    }

    if (m_shader == nullptr) {
      LOG_WARN("[GridPass] Skipping execute: shaders::grid is not available");
      return;
    }

    auto scene = SceneManager::get()->get_active_scene();
    if (scene == nullptr) {
      LOG_WARN("[GridPass] Skipping execute: no active scene");
      return;
    }

    auto &world = scene->world();
    auto camera = rendering::select_main_camera(world);
    if (!camera.has_value()) {
      LOG_WARN("[GridPass] Skipping execute: no main camera selected");
      return;
    }

    auto renderer_api = m_render_target->renderer_api();

    m_scene_color->bind();
    renderer_api->enable_buffer_testing();
    renderer_api->enable_blend();
    renderer_api->set_blend_func(RendererAPI::BlendFactor::SrcAlpha,
                                 RendererAPI::BlendFactor::OneMinusSrcAlpha);
    renderer_api->depth(RendererAPI::DepthMode::LessEqual);
    renderer_api->disable_depth_write();

    m_shader->bind();

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    using namespace shader_bindings::engine_shaders_grid_axsl;

    m_shader->set_all(GridParams{
        .view = camera->camera->view_matrix,
        .projection = camera->camera->projection_matrix,
        .render_mode = k_surface_render_mode,
    });
#endif

    renderer_api->draw_indexed(m_grid_quad.vertex_array, m_grid_quad.draw_type);

    renderer_api->disable_depth_test();
#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    m_shader->set(GridUniform::render_mode, k_y_axis_render_mode);
#else
    m_shader->set_int("grid.render_mode", k_y_axis_render_mode);
#endif
    renderer_api->draw_indexed(m_grid_quad.vertex_array, m_grid_quad.draw_type);
    renderer_api->enable_depth_test();

    m_shader->unbind();
    renderer_api->enable_depth_write();
    renderer_api->depth(RendererAPI::DepthMode::Less);
    renderer_api->disable_blend();
    m_scene_color->unbind();
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "GridPass"; }

private:
  Framebuffer *m_scene_color = nullptr;
  Ref<Shader> m_shader;
  Mesh m_grid_quad = Mesh::quad(1.0f);
  bool m_active = true;
};

} // namespace astralix
