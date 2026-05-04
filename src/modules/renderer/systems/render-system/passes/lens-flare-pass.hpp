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

class LensFlarePass : public FramePass {
public:
  explicit LensFlarePass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~LensFlarePass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("lens_flare_shader");

    if (m_shader == nullptr) {
      LOG_WARN(
          "[LensFlarePass] Missing graph dependency: lens_flare_shader"
      );
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("LensFlarePass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    if (!scene_frame->lens_flare.enabled) {
      return;
    }

    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *lens_flare_resource = ctx.find_graph_image("lens_flare");

    if (scene_color_resource == nullptr || lens_flare_resource == nullptr ||
        m_shader == nullptr || m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_lens_flare_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*lens_flare_resource);

    auto scene_color_input = ctx.register_graph_image(
        "lens-flare.scene-color-input", *scene_color_resource
    );
    auto lens_flare_output = ctx.register_graph_image(
        "lens-flare.output", *lens_flare_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "lens-flare-pass",
            "lens-flare-pass",
            m_shader,
            0,
            "lens-flare-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        LensFlareResourcesResources::scene_input.binding_id,
        ImageViewRef{.image = scene_color_input}
    );

    const auto &lens_flare = scene_frame->lens_flare;
    rendering::record_shader_params(frame, bindings, LensFlareParamsParams{
        .intensity = lens_flare.intensity,
        .threshold = lens_flare.threshold,
        .ghost_count = lens_flare.ghost_count,
        .ghost_dispersal = lens_flare.ghost_dispersal,
        .ghost_weight = lens_flare.ghost_weight,
        .halo_radius = lens_flare.halo_radius,
        .halo_weight = lens_flare.halo_weight,
        .halo_thickness = lens_flare.halo_thickness,
        .chromatic_aberration = lens_flare.chromatic_aberration,
        .texel_size = glm::vec2(
            1.0f / static_cast<float>(extent.width),
            1.0f / static_cast<float>(extent.height)
        ),
    });

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "lens-flare-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "lens-flare-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "lens-flare-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = lens_flare_output},
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

  std::string name() const override { return "LensFlarePass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
