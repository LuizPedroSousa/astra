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

class SSGIHistoryStorePass : public FramePass {
public:
  explicit SSGIHistoryStorePass(
      rendering::ResolvedMeshDraw fullscreen_quad = {}
  )
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~SSGIHistoryStorePass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("ssgi_history_store_shader");

    if (m_shader == nullptr) {
      LOG_WARN(
          "[SSGIHistoryStorePass] Missing graph dependency: "
          "ssgi_history_store_shader"
      );
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("SSGIHistoryStorePass::record");
    const auto *ssgi_temporal_resource =
        ctx.find_graph_image("ssgi_temporal");
    const auto *ssgi_history_resource = ctx.find_graph_image("ssgi_history");

    if (ssgi_temporal_resource == nullptr || ssgi_history_resource == nullptr ||
        m_shader == nullptr || m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_ssgi_history_store_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*ssgi_history_resource);

    auto temporal_input = ctx.register_graph_image(
        "ssgi-history-store.temporal-input", *ssgi_temporal_resource
    );
    auto history_output = ctx.register_graph_image(
        "ssgi-history-store.output", *ssgi_history_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "ssgi-history-store-pass",
            "ssgi-history-store-pass",
            m_shader,
            0,
            "ssgi-history-store-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings, HistoryStoreResources::temporal_input.binding_id,
        ImageViewRef{.image = temporal_input}
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "ssgi-history-store-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "ssgi-history-store-pass.fullscreen-quad",
        m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "ssgi-history-store-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = history_output},
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

  std::string name() const override { return "SSGIHistoryStorePass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
