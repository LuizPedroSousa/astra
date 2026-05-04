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

class TAACompositePass : public FramePass {
public:
  explicit TAACompositePass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~TAACompositePass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("taa_composite_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[TAACompositePass] Missing graph dependency: taa_composite_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("TAACompositePass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    if (!scene_frame->taa.enabled) {
      return;
    }

    const auto *taa_output_resource = ctx.find_graph_image("taa_output");
    const auto *scene_color_resource = ctx.find_graph_image("scene_color");

    if (taa_output_resource == nullptr || scene_color_resource == nullptr ||
        m_shader == nullptr || m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_taa_composite_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*scene_color_resource);

    auto taa_output_input = ctx.register_graph_image(
        "taa-composite.taa-output-input", *taa_output_resource
    );
    auto scene_color_target = ctx.register_graph_image(
        "taa-composite.scene-color-target", *scene_color_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "taa-composite-pass",
            "taa-composite-pass",
            m_shader,
            0,
            "taa-composite-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        TAACompositeResourcesResources::taa_output.binding_id,
        ImageViewRef{.image = taa_output_input}
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "taa-composite-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "taa-composite-pass.fullscreen-quad",
        m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "taa-composite-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = scene_color_target},
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

  std::string name() const override { return "TAACompositePass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
