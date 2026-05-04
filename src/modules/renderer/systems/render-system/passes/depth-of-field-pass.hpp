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

class DepthOfFieldPass : public FramePass {
public:
  explicit DepthOfFieldPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~DepthOfFieldPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("depth_of_field_shader");

    if (m_shader == nullptr) {
      LOG_WARN(
          "[DepthOfFieldPass] Missing graph dependency: depth_of_field_shader"
      );
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("DepthOfFieldPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    if (!scene_frame->depth_of_field.enabled) {
      return;
    }

    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *g_position_resource = ctx.find_graph_image("g_position");
    const auto *depth_of_field_resource =
        ctx.find_graph_image("depth_of_field");

    if (scene_color_resource == nullptr || g_position_resource == nullptr ||
        depth_of_field_resource == nullptr || m_shader == nullptr ||
        m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_depth_of_field_axsl;

    auto &frame = ctx.frame();
    const auto &dof_desc = depth_of_field_resource->get_graph_image()->desc;
    const auto extent = ctx.graph_image_extent(*depth_of_field_resource);

    auto scene_color_input = ctx.register_graph_image(
        "depth-of-field.scene-color-input", *scene_color_resource
    );
    auto g_position_input = ctx.register_graph_image(
        "depth-of-field.g-position-input", *g_position_resource
    );
    auto depth_of_field_output = ctx.register_graph_image(
        "depth-of-field.output", *depth_of_field_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "depth-of-field-pass",
            "depth-of-field-pass",
            m_shader,
            0,
            "depth-of-field-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        DepthOfFieldResourcesResources::scene_color.binding_id,
        ImageViewRef{.image = scene_color_input}
    );
    frame.add_sampled_image_binding(
        bindings,
        DepthOfFieldResourcesResources::g_position.binding_id,
        ImageViewRef{.image = g_position_input}
    );

    rendering::record_shader_params(
        frame, bindings,
        CameraParamsParams{
            .view = scene_frame->main_camera->view,
        }
    );

    rendering::record_shader_params(
        frame, bindings,
        DepthOfFieldParamsParams{
            .focus_distance = scene_frame->depth_of_field.focus_distance,
            .focus_range = scene_frame->depth_of_field.focus_range,
            .max_blur_radius = scene_frame->depth_of_field.max_blur_radius,
            .sample_count = scene_frame->depth_of_field.sample_count,
            .texel_size = glm::vec2(
                1.0f / static_cast<float>(dof_desc.width),
                1.0f / static_cast<float>(dof_desc.height)
            ),
        }
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "depth-of-field-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "depth-of-field-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "depth-of-field-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = depth_of_field_output},
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

  std::string name() const override { return "DepthOfFieldPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
