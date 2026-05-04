#pragma once

#include "framebuffer.hpp"
#include "glm/vec2.hpp"
#include "log.hpp"
#include "render-pass.hpp"
#include "resources/texture.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-frame.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class SSRPass : public FramePass {
public:
  explicit SSRPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~SSRPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("ssr_shader");
    m_noise_texture = ctx.require_texture_2d("noise_texture");

    if (m_shader == nullptr || m_noise_texture == nullptr) {
      LOG_WARN("[SSRPass] Missing graph dependencies");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("SSRPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    const auto *g_position_resource = ctx.find_graph_image("g_position");
    const auto *g_normal_resource = ctx.find_graph_image("g_normal");
    const auto *g_entity_id_resource = ctx.find_graph_image("g_entity_id");
    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *ssr_resource = ctx.find_graph_image("ssr");

    if (g_position_resource == nullptr || g_normal_resource == nullptr ||
        g_entity_id_resource == nullptr || scene_color_resource == nullptr ||
        ssr_resource == nullptr || m_shader == nullptr ||
        m_noise_texture == nullptr ||
        m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_ssr_axsl;

    auto &frame = ctx.frame();
    const auto &ssr_desc = ssr_resource->get_graph_image()->desc;
    const auto extent = ctx.graph_image_extent(*ssr_resource);

    auto g_position =
        ctx.register_graph_image("ssr.g-position", *g_position_resource);
    auto g_normal =
        ctx.register_graph_image("ssr.g-normal", *g_normal_resource);
    auto g_entity_id =
        ctx.register_graph_image("ssr.g-entity-id", *g_entity_id_resource);
    auto scene_color =
        ctx.register_graph_image("ssr.scene-color", *scene_color_resource);
    auto noise_image = frame.register_texture_2d(
        "ssr.noise-texture", m_noise_texture
    );
    auto ssr_output = ctx.register_graph_image("ssr.output", *ssr_resource);

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "ssr-pass",
            "ssr-pass",
            m_shader,
            0,
            "ssr-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings, GBufferResources::g_position.binding_id,
        ImageViewRef{.image = g_position}
    );
    frame.add_sampled_image_binding(
        bindings, GBufferResources::g_normal.binding_id,
        ImageViewRef{.image = g_normal}
    );
    frame.add_sampled_image_binding(
        bindings, GBufferResources::g_entity_id.binding_id,
        ImageViewRef{.image = g_entity_id}
    );
    frame.add_sampled_image_binding(
        bindings, GBufferResources::scene_color.binding_id,
        ImageViewRef{.image = scene_color}
    );
    frame.add_sampled_image_binding(
        bindings, SSRResources::noise_texture.binding_id,
        ImageViewRef{.image = noise_image}
    );

    rendering::record_shader_params(frame, bindings, GBufferParams{});
    rendering::record_shader_params(frame, bindings, CameraParams{
                                                         .view = scene_frame->main_camera->view,
                                                         .projection = scene_frame->main_camera->projection,
                                                         .position = scene_frame->main_camera->position,
                                                     });
    rendering::record_shader_params(frame, bindings, SSRParams{
                                                         .enabled = scene_frame->ssr.enabled,
                                                         .intensity = scene_frame->ssr.intensity,
                                                         .max_distance = scene_frame->ssr.max_distance,
                                                         .thickness = scene_frame->ssr.thickness,
                                                         .max_steps = scene_frame->ssr.max_steps,
                                                         .stride = scene_frame->ssr.stride,
                                                         .roughness_cutoff = scene_frame->ssr.roughness_cutoff,
                                                         .noise_scale = glm::vec2(
                                                             static_cast<float>(ssr_desc.width) / 4.0f,
                                                             static_cast<float>(ssr_desc.height) / 4.0f),
                                                     });

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "ssr-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "ssr-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "ssr-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = ssr_output},
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

  std::string name() const override { return "SSRPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  Ref<Texture2D> m_noise_texture = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
