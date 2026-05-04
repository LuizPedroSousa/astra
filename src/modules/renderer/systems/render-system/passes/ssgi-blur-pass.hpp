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

class SSGIBlurPass : public FramePass {
public:
  explicit SSGIBlurPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~SSGIBlurPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("ssgi_blur_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[SSGIBlurPass] Missing graph dependency: ssgi_blur_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("SSGIBlurPass::record");
    const auto *g_position_resource = ctx.find_graph_image("g_position");
    const auto *g_normal_resource = ctx.find_graph_image("g_normal");
    const auto *ssgi_resource = ctx.find_graph_image("ssgi");
    const auto *ssgi_blur_resource = ctx.find_graph_image("ssgi_blur");

    if (g_position_resource == nullptr || g_normal_resource == nullptr ||
        ssgi_resource == nullptr || ssgi_blur_resource == nullptr ||
        m_shader == nullptr ||
        m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_ssgi_blur_axsl;

    auto &frame = ctx.frame();
    const auto &blur_desc = ssgi_blur_resource->get_graph_image()->desc;
    const auto extent = ctx.graph_image_extent(*ssgi_blur_resource);

    auto ssgi_input =
        ctx.register_graph_image("ssgi-blur.ssgi-input", *ssgi_resource);
    auto g_position = ctx.register_graph_image(
        "ssgi-blur.g-position", *g_position_resource
    );
    auto g_normal =
        ctx.register_graph_image("ssgi-blur.g-normal", *g_normal_resource);
    auto blur_output = ctx.register_graph_image(
        "ssgi-blur.output", *ssgi_blur_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "ssgi-blur-pass",
            "ssgi-blur-pass",
            m_shader,
            0,
            "ssgi-blur-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings, BlurResourcesResources::ssgi_input.binding_id,
        ImageViewRef{.image = ssgi_input}
    );
    frame.add_sampled_image_binding(
        bindings, BlurResourcesResources::g_position.binding_id,
        ImageViewRef{.image = g_position}
    );
    frame.add_sampled_image_binding(
        bindings, BlurResourcesResources::g_normal.binding_id,
        ImageViewRef{.image = g_normal}
    );

    rendering::record_shader_params(frame, bindings, BlurParamsParams{
        .texel_size = glm::vec2(
            1.0f / static_cast<float>(blur_desc.width),
            1.0f / static_cast<float>(blur_desc.height)),
    });

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "ssgi-blur-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "ssgi-blur-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "ssgi-blur-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = blur_output},
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

  std::string name() const override { return "SSGIBlurPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
