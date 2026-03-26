#pragma once

#include "framebuffer.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "render-pass.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/mesh.hpp"
#include "shaders/engine_shaders_g_buffer_axsl.hpp"
#include "systems/render-system/light-frame.hpp"
#include "systems/render-system/material-binding.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/scene-selection.hpp"

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix {

class LightingPass : public RenderPass {
public:
  LightingPass() = default;
  ~LightingPass() override = default;

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
    }
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    ASTRA_PROFILE_N("LightingPass Update");

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    auto scene = SceneManager::get()->get_active_scene();
    if (scene == nullptr) {
      LOG_WARN("[LightingPass] Skipping execute: no active scene");
      return;
    }

    if (m_shadow_map == nullptr) {
      LOG_WARN("[LightingPass] Skipping execute: shadow_map framebuffer is not "
               "available");
      return;
    }

    auto &world = scene->world();
    auto camera = rendering::select_main_camera(world);
    if (!camera.has_value()) {
      LOG_WARN("[LightingPass] Skipping execute: no main camera selected");
      return;
    }

    auto renderer_api = m_render_target->renderer_api();
    const auto backend = renderer_api->get_backend();
    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        backend, {"shaders::lighting"});

    auto shader =
        resource_manager()->get_by_descriptor_id<Shader>("shaders::lighting");
    if (shader == nullptr) {
      LOG_WARN(
          "[LightingPass] Skipping execute: failed to load shaders::lighting");
      return;
    }

    const auto light_frame = rendering::collect_light_frame(world);
    rendering::ensure_mesh_uploaded(m_fullscreen_quad, m_render_target);
    shader->bind();

    using namespace shader_bindings::engine_shaders_light_axsl;

    int32_t texture_unit = 0;
    int32_t shadow_map_slot = -1;

    if (m_shadow_map != nullptr) {
      shadow_map_slot = texture_unit;
      renderer_api->bind_texture_2d(m_shadow_map->get_depth_attachment_id(),
                                    texture_unit);
      texture_unit++;
    }

    auto color_attachments = m_g_buffer->get_color_attachments();
    for (uint32_t i = 0; i < color_attachments.size(); i++) {
      renderer_api->bind_texture_2d(color_attachments[i], texture_unit + i);
    }

    shader->set_all(rendering::build_deferred_light_params(
        light_frame, shadow_map_slot, texture_unit, texture_unit + 1,
        texture_unit + 2));
    shader->set(CameraUniform::position, camera->transform->position);

    m_scene_color->bind();
    renderer_api->disable_depth_test();
    renderer_api->draw_indexed(m_fullscreen_quad.vertex_array,
                               m_fullscreen_quad.draw_type);
    renderer_api->enable_depth_test();
    shader->unbind();
    m_scene_color->unbind();
#endif
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const noexcept override { return "LightingPass"; }

private:
  Framebuffer *m_shadow_map = nullptr;
  Framebuffer *m_scene_color = nullptr;
  Framebuffer *m_g_buffer = nullptr;
  Mesh m_fullscreen_quad = Mesh::quad(1.0f);
};

} // namespace astralix
