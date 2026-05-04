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

class MotionBlurPass : public FramePass {
public:
  explicit MotionBlurPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~MotionBlurPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("motion_blur_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[MotionBlurPass] Missing graph dependency: motion_blur_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("MotionBlurPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    if (!scene_frame->motion_blur.enabled) {
      return;
    }

    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *g_velocity_resource = ctx.find_graph_image("g_velocity");
    const auto *g_position_resource = ctx.find_graph_image("g_position");
    const auto *motion_blur_resource = ctx.find_graph_image("motion_blur");

    if (scene_color_resource == nullptr || g_velocity_resource == nullptr ||
        g_position_resource == nullptr || motion_blur_resource == nullptr ||
        m_shader == nullptr || m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_motion_blur_axsl;

    auto &frame = ctx.frame();
    const auto &motion_blur_desc =
        motion_blur_resource->get_graph_image()->desc;
    const auto extent = ctx.graph_image_extent(*motion_blur_resource);

    auto scene_color_input = ctx.register_graph_image(
        "motion-blur.scene-color-input", *scene_color_resource
    );
    auto g_velocity_input = ctx.register_graph_image(
        "motion-blur.g-velocity-input", *g_velocity_resource
    );
    auto g_position_input = ctx.register_graph_image(
        "motion-blur.g-position-input", *g_position_resource
    );
    auto motion_blur_output = ctx.register_graph_image(
        "motion-blur.output", *motion_blur_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "motion-blur-pass",
            "motion-blur-pass",
            m_shader,
            0,
            "motion-blur-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        MotionBlurResourcesResources::scene_color.binding_id,
        ImageViewRef{.image = scene_color_input}
    );
    frame.add_sampled_image_binding(
        bindings,
        MotionBlurResourcesResources::g_velocity.binding_id,
        ImageViewRef{.image = g_velocity_input}
    );
    frame.add_sampled_image_binding(
        bindings,
        MotionBlurResourcesResources::g_position.binding_id,
        ImageViewRef{.image = g_position_input}
    );

    rendering::record_shader_params(
        frame, bindings,
        MotionBlurParamsParams{
            .intensity = scene_frame->motion_blur.intensity,
            .max_samples = scene_frame->motion_blur.max_samples,
            .depth_threshold = scene_frame->motion_blur.depth_threshold,
            .texel_size = glm::vec2(
                1.0f / static_cast<float>(motion_blur_desc.width),
                1.0f / static_cast<float>(motion_blur_desc.height)
            ),
        }
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "motion-blur-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "motion-blur-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "motion-blur-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = motion_blur_output},
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

  std::string name() const override { return "MotionBlurPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
