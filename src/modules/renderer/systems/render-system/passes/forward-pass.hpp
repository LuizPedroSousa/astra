#pragma once

#include "framebuffer.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "render-pass.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "systems/render-system/light-frame.hpp"
#include "systems/render-system/material-binding.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-frame.hpp"
#include "systems/render-system/scene-selection.hpp"
#include <unordered_map>

namespace astralix {

class ForwardPass : public RenderPass {
public:
  ForwardPass() = default;
  ~ForwardPass() override = default;

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
          break;
        }

        default:
          break;
      }
    }

    if (m_scene_color == nullptr || m_shadow_map == nullptr) {
      set_enabled(false);
    }
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    ASTRA_PROFILE_N("ForwardPass Update");

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    auto scene = SceneManager::get()->get_active_scene();
    if (scene == nullptr) {
      LOG_WARN("[ForwardPass] Skipping execute: no active scene");
      return;
    }

    if (m_shadow_map == nullptr) {
      LOG_WARN("[ForwardPass] Skipping execute: shadow_map framebuffer is not "
               "available");
      return;
    }

    auto &world = scene->world();
    if (!rendering::has_renderables(world) ||
        world.count<rendering::Renderable, scene::Transform,
                    rendering::ShaderBinding>() == 0u) {
      LOG_WARN(
          "[ForwardPass] Skipping execute: scene has no renderables with "
          "shader bindings");
      return;
    }

    auto camera = rendering::select_main_camera(world);
    if (!camera.has_value()) {
      LOG_WARN("[ForwardPass] Skipping execute: no main camera selected");
      return;
    }

    const auto light_frame = rendering::collect_light_frame(world);
    rendering::RenderFrameData frame =
        rendering::collect_render_frame(world, m_runtime_store);
    if (frame.packets.empty()) {
      LOG_WARN("[ForwardPass] Skipping execute: no render packets collected");
      return;
    }

    auto renderer_api = m_render_target->renderer_api();
    const auto backend = renderer_api->get_backend();

    std::unordered_map<ResourceDescriptorID, Ref<Shader>> shader_cache;
    shader_cache.reserve(frame.packets.size());

    m_scene_color->bind();
    renderer_api->enable_buffer_testing();
    renderer_api->depth(RendererAPI::DepthMode::Less);

    using namespace shader_bindings::engine_shaders_lighting_forward_axsl;

    for (const auto &packet : frame.packets) {
      if (packet.transform == nullptr ||
          (packet.model_ref == nullptr && packet.mesh_set == nullptr) ||
          packet.shader == nullptr) {
        continue;
      }

      auto [shader_it, inserted] =
          shader_cache.emplace(packet.shader->shader, nullptr);
      if (inserted) {
        resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
            backend, {packet.shader->shader});
        shader_it->second = resource_manager()->get_by_descriptor_id<Shader>(
            packet.shader->shader);
      }

      auto shader = shader_it->second;
      if (shader == nullptr) {
        continue;
      }

      shader->bind();

      const auto material_binding = rendering::bind_material_slots(
          renderer_api, shader, packet.model_ref, packet.materials,
          packet.textures);
      if (material_binding.diffuse_slot < 0 ||
          material_binding.specular_slot < 0) {
        shader->unbind();
        continue;
      }

      const int shadow_map_slot = material_binding.next_texture_slot;
      renderer_api->bind_texture_2d(m_shadow_map->get_depth_attachment_id(),
                                    shadow_map_slot);

      shader->set_all(CameraParams{
          .view = camera->camera->view_matrix,
          .projection = camera->camera->projection_matrix,
          .position = camera->transform->position,
      });
      shader->set_all(EntityParams{
          .use_instancing = false,
          .g_model = packet.transform->matrix,
      });
      shader->set_all(rendering::build_forward_light_params(
          light_frame, material_binding, shadow_map_slot));

      rendering::for_each_render_mesh(
          packet.model_ref, packet.mesh_set, m_render_target, [&](Mesh &mesh) {
            renderer_api->draw_indexed(mesh.vertex_array, mesh.draw_type);
          });

      shader->unbind();
    }

    m_scene_color->unbind();
#endif
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "ForwardPass"; }

private:
  Framebuffer *m_shadow_map = nullptr;
  Framebuffer *m_scene_color = nullptr;
  rendering::RenderRuntimeStore m_runtime_store;
};

} // namespace astralix
