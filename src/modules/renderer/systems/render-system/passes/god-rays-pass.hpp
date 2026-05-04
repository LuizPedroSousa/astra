#pragma once

#include "framebuffer.hpp"
#include "log.hpp"
#include "render-pass.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/light-frame.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-frame.hpp"
#include "trace.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class GodRaysPass : public FramePass {
public:
  explicit GodRaysPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~GodRaysPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("god_rays_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[GodRaysPass] Missing graph dependency: god_rays_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("GodRaysPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    if (!scene_frame->god_rays.enabled) {
      return;
    }

    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *g_position_resource = ctx.find_graph_image("g_position");
    const auto *god_rays_resource = ctx.find_graph_image("god_rays");

    if (scene_color_resource == nullptr || g_position_resource == nullptr ||
        god_rays_resource == nullptr || m_shader == nullptr ||
        m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_god_rays_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*god_rays_resource);

    auto scene_color_input = ctx.register_graph_image(
        "god-rays.scene-color-input", *scene_color_resource
    );
    auto g_position_input = ctx.register_graph_image(
        "god-rays.g-position-input", *g_position_resource
    );
    auto god_rays_output = ctx.register_graph_image(
        "god-rays.output", *god_rays_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "god-rays-pass",
            "god-rays-pass",
            m_shader,
            0,
            "god-rays-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        GodRaysResourcesResources::scene_color.binding_id,
        ImageViewRef{.image = scene_color_input}
    );
    frame.add_sampled_image_binding(
        bindings,
        GodRaysResourcesResources::g_position.binding_id,
        ImageViewRef{.image = g_position_input}
    );

    const auto &god_rays = scene_frame->god_rays;
    const auto &light_frame = scene_frame->light_frame;

    GodRaysParamsParams god_rays_params{};
    god_rays_params.directional.exposure.ambient = light_frame.directional.ambient;
    god_rays_params.directional.exposure.diffuse = light_frame.directional.diffuse;
    god_rays_params.directional.exposure.specular = light_frame.directional.specular;
    god_rays_params.directional.position = light_frame.directional.position;
    god_rays_params.directional.direction = light_frame.directional.direction;

    god_rays_params.view_matrix = scene_frame->main_camera->view;
    god_rays_params.projection_matrix = scene_frame->main_camera->projection;
    god_rays_params.camera_position = scene_frame->main_camera->position;
    god_rays_params.intensity = god_rays.intensity;
    god_rays_params.decay = god_rays.decay;
    god_rays_params.density = god_rays.density;
    god_rays_params.weight = god_rays.weight;
    god_rays_params.threshold = god_rays.threshold;
    god_rays_params.samples = god_rays.samples;

    rendering::record_shader_params(frame, bindings, god_rays_params);

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "god-rays-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "god-rays-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "god-rays-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = god_rays_output},
        .load_op = AttachmentLoadOp::Clear,
        .store_op = AttachmentStoreOp::Store,
        .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
    });

    recorder.begin_rendering(info);
    recorder.bind_pipeline(pipeline);
    recorder.bind_binding_group(bindings);
    recorder.bind_vertex_buffer(quad_buffer);
    recorder.bind_index_buffer(quad_buffer, IndexType::Uint32);
    recorder.draw_indexed(DrawIndexedArgs{
        .index_count = m_fullscreen_quad.index_count,
    });
    recorder.end_rendering();
  }

  std::string name() const override { return "GodRaysPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
