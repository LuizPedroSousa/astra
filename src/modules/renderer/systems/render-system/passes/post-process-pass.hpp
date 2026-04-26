#pragma once

#include "framebuffer.hpp"
#include "render-pass.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-frame.hpp"
#include "trace.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class PostProcessPass : public FramePass {
public:
  explicit PostProcessPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~PostProcessPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("hdr_shader");
    if (m_shader == nullptr || m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      LOG_WARN("[PostProcessPass] Missing graph dependency: hdr_shader");
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("PostProcessPass::record");

    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *bloom_resource = ctx.find_graph_image("bloom");
    const auto *present_resource = ctx.find_graph_image("present");

    if (m_shader == nullptr || scene_color_resource == nullptr ||
        bloom_resource == nullptr || present_resource == nullptr ||
        m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    auto &frame = ctx.frame();

    const auto extent = ctx.graph_image_extent(*scene_color_resource);

    auto scene_color_image = ctx.register_graph_image(
        "post-process.scene-color", *scene_color_resource
    );
    auto bloom_image =
        ctx.register_graph_image("post-process.bloom", *bloom_resource);

    auto present_image =
        ctx.register_graph_image("post-process.present", *present_resource);

    using namespace shader_bindings::engine_shaders_hdr_axsl;

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "post-process";
    pipeline_desc.raster.cull_mode = CullMode::None;
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;
    pipeline_desc.blend_attachments = {BlendAttachmentState::replace()};

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "post-process",
            "post-process-pass",
            m_shader,
            0,
            "post-process",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings, GlobalsResources::screen_texture.binding_id, ImageViewRef{.image = scene_color_image}
    );
    frame.add_sampled_image_binding(
        bindings, GlobalsResources::bloom_texture.binding_id, ImageViewRef{.image = bloom_image}
    );

    constexpr float k_bloom_strength = 0.12f;
    constexpr float k_gamma = 2.2f;
    constexpr float k_exposure = 0.7f;
    rendering::record_shader_params(frame, bindings, GlobalsParams{
                                                         .bloom_strength = k_bloom_strength,
                                                         .gamma = k_gamma,
                                                         .exposure = k_exposure,
                                                     });

    const auto fullscreen_quad = frame.register_vertex_array(
        "post-process.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "post-process";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = present_image},
        .load_op = AttachmentLoadOp::Clear,
        .store_op = AttachmentStoreOp::Store,
        .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
    });

    recorder.begin_rendering(info);
    recorder.bind_pipeline(pipeline);
    recorder.bind_binding_group(bindings);
    recorder.bind_vertex_buffer(fullscreen_quad);
    recorder.bind_index_buffer(fullscreen_quad, IndexType::Uint32);
    recorder.draw_indexed(DrawIndexedArgs{
        .index_count = m_fullscreen_quad.index_count,
    });
    recorder.end_rendering();
  }

  bool has_side_effects() const override { return true; }

  std::string name() const override { return "PostProcessPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
