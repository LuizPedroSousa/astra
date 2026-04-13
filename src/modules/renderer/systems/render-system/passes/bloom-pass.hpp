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

class BloomPass : public FramePass {
public:
  explicit BloomPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~BloomPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("bloom_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[BloomPass] Missing graph dependency: bloom_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("BloomPass::record");
    const auto *bloom_extract = ctx.find_graph_image("bloom_extract");
    const auto *bloom_resource = ctx.find_graph_image("bloom");

    if (bloom_extract == nullptr || bloom_resource == nullptr ||
        m_shader == nullptr ||
        m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_bloom_axsl;

    auto &frame = ctx.frame();
    const auto &bloom_desc = bloom_resource->get_graph_image()->desc;
    const auto extent = ctx.graph_image_extent(*bloom_resource);

    auto bright_attachment = ctx.register_graph_image(
        "bloom.bright-input", *bloom_extract
    );
    auto bloom_color = ctx.register_graph_image("bloom.output", *bloom_resource);

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "bloom-pass",
            "bloom-pass",
            m_shader,
            0,
            "bloom-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings, BloomResources::bright_texture.binding_id,
        ImageViewRef{.image = bright_attachment}
    );

    rendering::record_shader_params(frame, bindings, BloomParams{
                                                         .texel_size = glm::vec2(1.0f / static_cast<float>(bloom_desc.width), 1.0f / static_cast<float>(bloom_desc.height)),
                                                     });

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "bloom-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "bloom-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "bloom-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = bloom_color},
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

  std::string name() const override { return "BloomPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
