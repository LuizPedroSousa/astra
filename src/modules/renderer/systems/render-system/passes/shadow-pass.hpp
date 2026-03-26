#pragma once

#include "framebuffer.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "render-pass.hpp"
#include "systems/render-system/light-frame.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

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
      LOG_WARN("[ShadowPass] Skipping setup: shadow_map framebuffer is not "
               "available");
      return;
    }

    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        m_render_target->renderer_api()->get_backend(),
        {"shaders::shadow_map"});
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    if (m_shadow_mapping_framebuffer == nullptr) {
      LOG_WARN("[ShadowPass] Skipping execute: shadow_map framebuffer is not "
               "available");
      return;
    }

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    auto scene = SceneManager::get()->get_active_scene();
    if (scene == nullptr) {
      LOG_WARN("[ShadowPass] Skipping execute: no active scene");
      return;
    }

    auto &world = scene->world();
    if (!rendering::has_renderables(world)) {
      LOG_WARN("[ShadowPass] Skipping execute: scene has no renderables");
      return;
    }

    const auto light_frame = rendering::collect_light_frame(world);
    if (!light_frame.directional.valid) {
      LOG_WARN("[ShadowPass] Skipping execute: no valid directional light");
      return;
    }

    const auto backend = m_render_target->renderer_api()->get_backend();
    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        backend, {"shaders::shadow_map"});

    auto shader =
        resource_manager()->get_by_descriptor_id<Shader>("shaders::shadow_map");
    if (shader == nullptr) {
      LOG_WARN(
          "[ShadowPass] Skipping execute: failed to load shaders::shadow_map");
      return;
    }

    const auto light_space_matrix = light_frame.directional.light_space_matrix;

    auto renderer_api = m_render_target->renderer_api();

    m_shadow_mapping_framebuffer->bind();
    renderer_api->enable_buffer_testing();
    renderer_api->clear_buffers();
    renderer_api->clear_color();
    renderer_api->cull_face(RendererAPI::CullFaceMode::Front);

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

          shader->bind();

          using namespace shader_bindings::engine_shaders_shadow_map_axsl;
          shader->set_all(LightParams{
              .light_space_matrix = light_space_matrix,
              .g_model = transform.matrix,
          });

          rendering::for_each_render_mesh(
              model_ref, mesh_set, m_render_target, [&](Mesh &mesh) {
                renderer_api->draw_indexed(mesh.vertex_array, mesh.draw_type);
              });

          shader->unbind();
        });

    renderer_api->cull_face(RendererAPI::CullFaceMode::Back);

    m_shadow_mapping_framebuffer->unbind();
    m_render_target->framebuffer()->bind();
#endif
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "ShadowPass"; }

private:
  Framebuffer *m_shadow_mapping_framebuffer = nullptr;
};

} // namespace astralix
