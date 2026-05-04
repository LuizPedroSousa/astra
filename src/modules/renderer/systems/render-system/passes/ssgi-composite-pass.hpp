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

class SSGICompositePass : public FramePass {
public:
  explicit SSGICompositePass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~SSGICompositePass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("ssgi_composite_shader");

    if (m_shader == nullptr) {
      LOG_WARN(
          "[SSGICompositePass] Missing graph dependency: ssgi_composite_shader"
      );
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("SSGICompositePass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *ssgi_temporal_resource =
        ctx.find_graph_image("ssgi_temporal");
    const auto *g_position_resource = ctx.find_graph_image("g_position");
    const auto *g_normal_resource = ctx.find_graph_image("g_normal");
    const auto *g_entity_id_resource = ctx.find_graph_image("g_entity_id");

    if (scene_color_resource == nullptr || ssgi_temporal_resource == nullptr ||
        g_position_resource == nullptr || g_normal_resource == nullptr ||
        g_entity_id_resource == nullptr || m_shader == nullptr ||
        m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_ssgi_composite_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*scene_color_resource);

    auto scene_color_target = ctx.register_graph_image(
        "ssgi-composite.scene-color-target", *scene_color_resource
    );
    auto ssgi_input = ctx.register_graph_image(
        "ssgi-composite.ssgi-input", *ssgi_temporal_resource
    );
    auto g_position = ctx.register_graph_image(
        "ssgi-composite.g-position", *g_position_resource
    );
    auto g_normal = ctx.register_graph_image(
        "ssgi-composite.g-normal", *g_normal_resource
    );
    auto g_entity_id = ctx.register_graph_image(
        "ssgi-composite.g-entity-id", *g_entity_id_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "ssgi-composite-pass",
            "ssgi-composite-pass",
            m_shader,
            0,
            "ssgi-composite-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings, CompositeResourcesResources::ssgi_input.binding_id,
        ImageViewRef{.image = ssgi_input}
    );
    frame.add_sampled_image_binding(
        bindings, CompositeResourcesResources::g_position.binding_id,
        ImageViewRef{.image = g_position}
    );
    frame.add_sampled_image_binding(
        bindings, CompositeResourcesResources::g_normal.binding_id,
        ImageViewRef{.image = g_normal}
    );
    frame.add_sampled_image_binding(
        bindings, CompositeResourcesResources::g_entity_id.binding_id,
        ImageViewRef{.image = g_entity_id}
    );

    rendering::record_shader_params(frame, bindings, CompositeParamsParams{
                                                         .enabled = scene_frame->ssgi.enabled,
                                                         .intensity = scene_frame->ssgi.intensity,
                                                     });

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "ssgi-composite-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;
    pipeline_desc.blend_attachments = {{
        .enabled = true,
        .src = BlendFactor::One,
        .dst = BlendFactor::One,
    }};

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "ssgi-composite-pass.fullscreen-quad",
        m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "ssgi-composite-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = scene_color_target},
        .load_op = AttachmentLoadOp::Load,
        .store_op = AttachmentStoreOp::Store,
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

  std::string name() const override { return "SSGICompositePass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
