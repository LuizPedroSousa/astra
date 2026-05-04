#pragma once

#include "framebuffer.hpp"
#include "log.hpp"
#include "render-pass.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-frame.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class SSGITemporalPass : public FramePass {
public:
  explicit SSGITemporalPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~SSGITemporalPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("ssgi_temporal_shader");

    if (m_shader == nullptr) {
      LOG_WARN(
          "[SSGITemporalPass] Missing graph dependency: ssgi_temporal_shader"
      );
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("SSGITemporalPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    const auto *ssgi_blur_resource = ctx.find_graph_image("ssgi_blur");
    const auto *ssgi_history_resource = ctx.find_graph_image("ssgi_history");
    const auto *ssgi_temporal_resource =
        ctx.find_graph_image("ssgi_temporal");
    const auto *g_position_resource = ctx.find_graph_image("g_position");
    const auto *g_normal_resource = ctx.find_graph_image("g_normal");
    const auto *g_geometric_normal_resource =
        ctx.find_graph_image("g_geometric_normal");
    const auto *g_entity_id_resource = ctx.find_graph_image("g_entity_id");
    const auto *g_velocity_resource = ctx.find_graph_image("g_velocity");

    if (ssgi_blur_resource == nullptr || ssgi_history_resource == nullptr ||
        ssgi_temporal_resource == nullptr || g_position_resource == nullptr ||
        g_normal_resource == nullptr ||
        g_geometric_normal_resource == nullptr ||
        g_entity_id_resource == nullptr || g_velocity_resource == nullptr ||
        m_shader == nullptr || m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_ssgi_temporal_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*ssgi_temporal_resource);

    auto current_ssgi = ctx.register_graph_image(
        "ssgi-temporal.current-ssgi", *ssgi_blur_resource
    );
    auto history_ssgi = ctx.register_graph_image(
        "ssgi-temporal.history-ssgi", *ssgi_history_resource
    );
    auto g_position = ctx.register_graph_image(
        "ssgi-temporal.g-position", *g_position_resource
    );
    auto g_normal = ctx.register_graph_image(
        "ssgi-temporal.g-normal", *g_normal_resource
    );
    auto g_geometric_normal = ctx.register_graph_image(
        "ssgi-temporal.g-geometric-normal",
        *g_geometric_normal_resource
    );
    auto g_entity_id = ctx.register_graph_image(
        "ssgi-temporal.g-entity-id", *g_entity_id_resource
    );
    auto g_velocity = ctx.register_graph_image(
        "ssgi-temporal.g-velocity", *g_velocity_resource
    );
    auto temporal_output = ctx.register_graph_image(
        "ssgi-temporal.output", *ssgi_temporal_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "ssgi-temporal-pass",
            "ssgi-temporal-pass",
            m_shader,
            0,
            "ssgi-temporal-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings, TemporalResourcesResources::current_ssgi.binding_id,
        ImageViewRef{.image = current_ssgi}
    );
    frame.add_sampled_image_binding(
        bindings, TemporalResourcesResources::history_ssgi.binding_id,
        ImageViewRef{.image = history_ssgi}
    );
    frame.add_sampled_image_binding(
        bindings, TemporalResourcesResources::g_position.binding_id,
        ImageViewRef{.image = g_position}
    );
    frame.add_sampled_image_binding(
        bindings, TemporalResourcesResources::g_normal.binding_id,
        ImageViewRef{.image = g_normal}
    );
    frame.add_sampled_image_binding(
        bindings, TemporalResourcesResources::g_geometric_normal.binding_id,
        ImageViewRef{.image = g_geometric_normal}
    );
    frame.add_sampled_image_binding(
        bindings, TemporalResourcesResources::g_entity_id.binding_id,
        ImageViewRef{.image = g_entity_id}
    );
    frame.add_sampled_image_binding(
        bindings, TemporalResourcesResources::g_velocity.binding_id,
        ImageViewRef{.image = g_velocity}
    );

    const auto &camera = *scene_frame->main_camera;
    rendering::record_shader_params(frame, bindings, TemporalParamsParams{
                                                         .current_view = camera.view,
                                                         .current_projection = camera.projection,
                                                         .previous_view = camera.previous_view,
                                                         .previous_projection = camera.previous_projection,
                                                         .temporal_enabled = scene_frame->ssgi.enabled && scene_frame->ssgi.temporal,
                                                         .has_history = camera.has_history,
                                                         .history_weight = scene_frame->ssgi.history_weight,
                                                         .position_reject_distance = scene_frame->ssgi.position_reject_distance,
                                                         .normal_reject_dot = scene_frame->ssgi.normal_reject_dot,
                                                     });

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "ssgi-temporal-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "ssgi-temporal-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "ssgi-temporal-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = temporal_output},
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

  std::string name() const override { return "SSGITemporalPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
