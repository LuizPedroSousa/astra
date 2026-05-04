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

class VolumetricTemporalPass : public FramePass {
public:
  explicit VolumetricTemporalPass(
      rendering::ResolvedMeshDraw fullscreen_quad = {}
  )
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~VolumetricTemporalPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("volumetric_temporal_shader");

    if (m_shader == nullptr) {
      LOG_WARN(
          "[VolumetricTemporalPass] Missing graph dependency: "
          "volumetric_temporal_shader"
      );
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("VolumetricTemporalPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    if (!scene_frame->volumetric.enabled) {
      return;
    }

    const auto *volumetric_blur_resource = ctx.find_graph_image("volumetric_blur");
    const auto *volumetric_history_resource = ctx.find_graph_image("volumetric_history");
    const auto *volumetric_temporal_resource = ctx.find_graph_image("volumetric_temporal");
    const auto *g_position_resource = ctx.find_graph_image("g_position");

    if (volumetric_blur_resource == nullptr ||
        volumetric_history_resource == nullptr ||
        volumetric_temporal_resource == nullptr ||
        g_position_resource == nullptr || m_shader == nullptr ||
        m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_volumetric_temporal_axsl;

    auto &frame = ctx.frame();
    const auto &output_desc = volumetric_temporal_resource->get_graph_image()->desc;
    const auto extent = ctx.graph_image_extent(*volumetric_temporal_resource);

    auto volumetric_current = ctx.register_graph_image(
        "volumetric-temporal.current", *volumetric_blur_resource
    );
    auto volumetric_history = ctx.register_graph_image(
        "volumetric-temporal.history", *volumetric_history_resource
    );
    auto g_position = ctx.register_graph_image(
        "volumetric-temporal.g-position", *g_position_resource
    );
    auto temporal_output = ctx.register_graph_image(
        "volumetric-temporal.output", *volumetric_temporal_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "volumetric-temporal-pass",
            "volumetric-temporal-pass",
            m_shader,
            0,
            "volumetric-temporal-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        VolumetricTemporalResourcesResources::volumetric_current.binding_id,
        ImageViewRef{.image = volumetric_current}
    );
    frame.add_sampled_image_binding(
        bindings,
        VolumetricTemporalResourcesResources::volumetric_history.binding_id,
        ImageViewRef{.image = volumetric_history}
    );
    frame.add_sampled_image_binding(
        bindings,
        VolumetricTemporalResourcesResources::g_position.binding_id,
        ImageViewRef{.image = g_position}
    );

    const auto &camera = *scene_frame->main_camera;
    glm::mat4 previous_view_projection = camera.previous_projection * camera.previous_view;

    rendering::record_shader_params(frame, bindings, VolumetricTemporalParamsParams{
        .previous_view_projection = previous_view_projection,
        .texel_size = glm::vec2(
            1.0f / static_cast<float>(output_desc.width),
            1.0f / static_cast<float>(output_desc.height)),
        .blend_weight = scene_frame->volumetric.temporal_blend_weight,
        .temporal_enabled = scene_frame->volumetric.temporal_enabled,
        .has_history = camera.has_history,
    });

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "volumetric-temporal-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "volumetric-temporal-pass.fullscreen-quad",
        m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "volumetric-temporal-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = temporal_output},
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

  std::string name() const override { return "VolumetricTemporalPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
