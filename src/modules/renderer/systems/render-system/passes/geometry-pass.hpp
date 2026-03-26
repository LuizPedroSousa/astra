#pragma once

#include "framebuffer.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "render-pass.hpp"
#include "renderer-api.hpp"
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

class GeometryPass : public RenderPass {
public:
  GeometryPass() = default;
  ~GeometryPass() override = default;

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;
    for (auto resource : resources) {
      switch (resource->desc.type) {
        case RenderGraphResourceType::Framebuffer: {
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
    ASTRA_PROFILE_N("GeometryPass Update");

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    auto scene = SceneManager::get()->get_active_scene();
    if (scene == nullptr) {
      LOG_WARN("[GeometryPass] Skipping execute: no active scene");
      return;
    }

    auto &world = scene->world();
    if (!rendering::has_renderables(world)) {
      LOG_WARN("[GeometryPass] Skipping execute: scene has no renderables");
      return;
    }

    auto camera = rendering::select_main_camera(world);
    if (!camera.has_value()) {
      LOG_WARN("[GeometryPass] Skipping execute: no main camera selected");
      return;
    }

    auto renderer_api = m_render_target->renderer_api();
    const auto light_frame = rendering::collect_light_frame(world);
    const auto backend = renderer_api->get_backend();
    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        backend, {"shaders::g_buffer"});

    auto shader =
        resource_manager()->get_by_descriptor_id<Shader>("shaders::g_buffer");
    if (shader == nullptr) {
      LOG_WARN(
          "[GeometryPass] Skipping execute: failed to load shaders::g_buffer");
      return;
    }

    using namespace shader_bindings::engine_shaders_g_buffer_axsl;

    m_g_buffer->bind();
    m_render_target->renderer_api()->clear_buffers(ClearBufferType::Color |
                                                   ClearBufferType::Depth);

    world.each<rendering::Renderable, scene::Transform>(
        [&](EntityID entity_id, rendering::Renderable &,
            scene::Transform &transform) {
          if (!world.active(entity_id)) {
            return;
          }

          auto entity = world.entity(entity_id);
          auto *model_ref = entity.get<rendering::ModelRef>();
          auto *mesh_set = entity.get<rendering::MeshSet>();
          if (model_ref == nullptr && mesh_set == nullptr) {
            return;
          }

          auto *materials = entity.get<rendering::MaterialSlots>();
          auto *textures = entity.get<rendering::TextureBindings>();

          shader->bind();

          const auto material_binding = rendering::bind_material_slots(
              renderer_api, shader, model_ref, materials, textures);
          if (material_binding.diffuse_slot < 0 ||
              material_binding.specular_slot < 0) {
            shader->unbind();
            return;
          }

          shader->set_all(CameraParams{
              .view = camera->camera->view_matrix,
              .projection = camera->camera->projection_matrix,
              .position = camera->transform->position,
          });

          shader->set_all(EntityParams{
              .use_instancing = false,
              .g_model = transform.matrix,
          });

          shader->set_all(rendering::build_gbuffer_light_params(
              light_frame, material_binding));

          rendering::for_each_render_mesh(
              model_ref, mesh_set, m_render_target, [&](Mesh &mesh) {
                m_render_target->renderer_api()->draw_indexed(mesh.vertex_array,
                                                              mesh.draw_type);
              });

          shader->unbind();
        });

    m_g_buffer->bind(FramebufferBindType::Read);
    m_scene_color->bind(FramebufferBindType::Draw);

    const FramebufferSpecification &spec = m_g_buffer->get_specification();
    m_scene_color->blit(spec.width, spec.height, FramebufferBlitType::Depth);

    m_g_buffer->unbind();
    m_scene_color->unbind();
#endif
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const noexcept override { return "GeometryPass"; }

private:
  Framebuffer *m_scene_color = nullptr;
  Framebuffer *m_g_buffer = nullptr;
};

} // namespace astralix
