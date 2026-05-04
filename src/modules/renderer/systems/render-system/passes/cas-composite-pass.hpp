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

class CASCompositePass : public FramePass {
public:
  explicit CASCompositePass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~CASCompositePass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("cas_composite_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[CASCompositePass] Missing graph dependency: cas_composite_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("CASCompositePass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    if (!scene_frame->cas.enabled) {
      return;
    }

    const auto *cas_resource = ctx.find_graph_image("cas");
    const auto *present_resource = ctx.find_graph_image("present");

    if (cas_resource == nullptr || present_resource == nullptr ||
        m_shader == nullptr || m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_cas_composite_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*present_resource);

    auto cas_input = ctx.register_graph_image(
        "cas-composite.cas-input", *cas_resource
    );
    auto present_target = ctx.register_graph_image(
        "cas-composite.present-target", *present_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "cas-composite-pass",
            "cas-composite-pass",
            m_shader,
            0,
            "cas-composite-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        CASCompositeResourcesResources::effect_input.binding_id,
        ImageViewRef{.image = cas_input}
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "cas-composite-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "cas-composite-pass.fullscreen-quad",
        m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "cas-composite-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = present_target},
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

  std::string name() const override { return "CASCompositePass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
