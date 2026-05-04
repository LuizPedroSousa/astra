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

class FilmGrainPass : public FramePass {
public:
  explicit FilmGrainPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~FilmGrainPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("film_grain_shader");

    if (m_shader == nullptr) {
      LOG_WARN(
          "[FilmGrainPass] Missing graph dependency: film_grain_shader"
      );
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("FilmGrainPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    if (!scene_frame->film_grain.enabled) {
      return;
    }

    const auto *present_resource = ctx.find_graph_image("present");
    const auto *film_grain_resource = ctx.find_graph_image("film_grain");

    if (present_resource == nullptr || film_grain_resource == nullptr ||
        m_shader == nullptr || m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_film_grain_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*film_grain_resource);

    auto present_input = ctx.register_graph_image(
        "film-grain.present-input", *present_resource
    );
    auto effect_output = ctx.register_graph_image(
        "film-grain.output", *film_grain_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "film-grain-pass",
            "film-grain-pass",
            m_shader,
            0,
            "film-grain-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        FilmGrainResourcesResources::screen_texture.binding_id,
        ImageViewRef{.image = present_input}
    );

    rendering::record_shader_params(
        frame, bindings,
        FilmGrainParamsParams{
            .intensity = scene_frame->film_grain.intensity,
            .time = scene_frame->film_grain.time,
        }
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "film-grain-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "film-grain-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "film-grain-pass";
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

  std::string name() const override { return "FilmGrainPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
