#pragma once

#include "framebuffer.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "render-pass.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/mesh.hpp"
#include "systems/render-system/material-binding.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/scene-selection.hpp"

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix {

class SkyboxPass : public RenderPass {
public:
  SkyboxPass() = default;
  ~SkyboxPass() override = default;

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
      LOG_WARN("[SkyboxPass] Skipping setup: scene_color framebuffer is not "
               "available");
      return;
    }

    rendering::ensure_mesh_uploaded(m_skybox_cube, m_render_target);
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    auto scene = SceneManager::get()->get_active_scene();

    if (scene == nullptr) {
      LOG_WARN("[SkyboxPass] Skipping execute: no active scene");
      return;
    }

    if (m_scene_color == nullptr) {
      LOG_WARN("[SkyboxPass] Skipping execute: scene_color framebuffer is not "
               "available");
      return;
    }

    auto &world = scene->world();
    auto camera = rendering::select_main_camera(world);
    auto skybox = rendering::select_skybox(world);

    if (!camera.has_value()) {
      LOG_WARN("[SkyboxPass] Skipping execute: no main camera selected");
      return;
    }

    if (!skybox.has_value()) {
      LOG_WARN("[SkyboxPass] Skipping execute: no skybox selected");
      return;
    }

    if (skybox->skybox == nullptr) {
      LOG_WARN("[SkyboxPass] Skipping execute: skybox asset is not available");
      return;
    }

    auto renderer_api = m_render_target->renderer_api();
    const auto backend = renderer_api->get_backend();
    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        backend, {skybox->skybox->shader});

    auto shader = resource_manager()->get_by_descriptor_id<Shader>(
        skybox->skybox->shader);
    if (shader == nullptr) {
      LOG_WARN("[SkyboxPass] Skipping execute: failed to load skybox shader '",
               skybox->skybox->shader, "'");
      return;
    }

    const int cubemap_slot =
        rendering::bind_texture_3d(renderer_api, skybox->skybox->cubemap, 0);
    if (cubemap_slot < 0) {
      LOG_WARN("[SkyboxPass] Skipping execute: failed to bind skybox cubemap");
      return;
    }

    m_scene_color->bind();
    renderer_api->disable_depth_write();
    renderer_api->depth(RendererAPI::DepthMode::LessEqual);

    shader->bind();

    const glm::mat4 view_without_translation =
        glm::mat4(glm::mat3(camera->camera->view_matrix));

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    using namespace shader_bindings::engine_shaders_skybox_axsl;

    shader->set_all(LightParams{
        .view_without_transformation = view_without_translation,
        .projection = camera->camera->projection_matrix,
    });
    shader->set_all(EntityParams{.skybox_map = cubemap_slot});
#endif

    renderer_api->draw_indexed(m_skybox_cube.vertex_array,
                               m_skybox_cube.draw_type);

    shader->unbind();
    renderer_api->enable_depth_write();
    renderer_api->depth(RendererAPI::DepthMode::Less);
    m_scene_color->unbind();
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "SkyboxPass"; }

private:
  Framebuffer *m_scene_color = nullptr;
  Mesh m_skybox_cube = Mesh::cube(2.0f);
};

} // namespace astralix
