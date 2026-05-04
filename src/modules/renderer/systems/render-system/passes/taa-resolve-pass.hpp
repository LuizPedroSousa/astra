#pragma once

#include "framebuffer.hpp"
#include "log.hpp"
#include "render-pass.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-frame.hpp"
#include "trace.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class TAAResolvePass : public FramePass {
public:
  explicit TAAResolvePass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~TAAResolvePass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("taa_resolve_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[TAAResolvePass] Missing graph dependency: taa_resolve_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("TAAResolvePass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    if (!scene_frame->taa.enabled) {
      return;
    }

    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *taa_history_resource = ctx.find_graph_image("taa_history");
    const auto *g_velocity_resource = ctx.find_graph_image("g_velocity");
    const auto *taa_output_resource = ctx.find_graph_image("taa_output");

    if (scene_color_resource == nullptr || taa_history_resource == nullptr ||
        g_velocity_resource == nullptr || taa_output_resource == nullptr ||
        m_shader == nullptr || m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_taa_resolve_axsl;

    auto &frame = ctx.frame();
    const auto &taa_desc = taa_output_resource->get_graph_image()->desc;
    const auto extent = ctx.graph_image_extent(*taa_output_resource);

    auto scene_color_input = ctx.register_graph_image(
        "taa-resolve.scene-color-input", *scene_color_resource
    );
    auto taa_history_input = ctx.register_graph_image(
        "taa-resolve.taa-history-input", *taa_history_resource
    );
    auto g_velocity_input = ctx.register_graph_image(
        "taa-resolve.g-velocity-input", *g_velocity_resource
    );
    auto taa_output = ctx.register_graph_image(
        "taa-resolve.output", *taa_output_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "taa-resolve-pass",
            "taa-resolve-pass",
            m_shader,
            0,
            "taa-resolve-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        TAAResourcesResources::scene_color.binding_id,
        ImageViewRef{.image = scene_color_input}
    );
    frame.add_sampled_image_binding(
        bindings,
        TAAResourcesResources::taa_history.binding_id,
        ImageViewRef{.image = taa_history_input}
    );
    frame.add_sampled_image_binding(
        bindings,
        TAAResourcesResources::g_velocity.binding_id,
        ImageViewRef{.image = g_velocity_input}
    );

    rendering::record_shader_params(
        frame, bindings,
        TAAParamsParams{
            .blend_factor = scene_frame->taa.blend_factor,
            .texel_size = glm::vec2(
                1.0f / static_cast<float>(taa_desc.width),
                1.0f / static_cast<float>(taa_desc.height)
            ),
        }
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "taa-resolve-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "taa-resolve-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "taa-resolve-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = taa_output},
        .load_op = AttachmentLoadOp::Clear,
        .store_op = AttachmentStoreOp::Store,
        .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
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

  std::string name() const override { return "TAAResolvePass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
