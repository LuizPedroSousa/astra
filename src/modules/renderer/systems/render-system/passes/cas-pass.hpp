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

class CASPass : public FramePass {
public:
  explicit CASPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~CASPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("cas_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[CASPass] Missing graph dependency: cas_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("CASPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    if (!scene_frame->cas.enabled) {
      return;
    }

    const auto *present_resource = ctx.find_graph_image("present");
    const auto *cas_resource = ctx.find_graph_image("cas");

    if (present_resource == nullptr || cas_resource == nullptr ||
        m_shader == nullptr || m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_cas_axsl;

    auto &frame = ctx.frame();
    const auto &cas_desc = cas_resource->get_graph_image()->desc;
    const auto extent = ctx.graph_image_extent(*cas_resource);

    auto present_input = ctx.register_graph_image(
        "cas.present-input", *present_resource
    );
    auto effect_output = ctx.register_graph_image(
        "cas.output", *cas_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "cas-pass",
            "cas-pass",
            m_shader,
            0,
            "cas-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        CASResourcesResources::screen_texture.binding_id,
        ImageViewRef{.image = present_input}
    );

    rendering::record_shader_params(
        frame, bindings,
        CASParamsParams{
            .sharpness = scene_frame->cas.sharpness,
            .contrast = scene_frame->cas.contrast,
            .sharpening_limit = scene_frame->cas.sharpening_limit,
            .texel_size = glm::vec2(
                1.0f / static_cast<float>(cas_desc.width),
                1.0f / static_cast<float>(cas_desc.height)
            ),
        }
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "cas-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "cas-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "cas-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = effect_output},
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

  std::string name() const override { return "CASPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
