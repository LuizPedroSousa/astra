#pragma once

#include "framebuffer.hpp"
#include "render-pass.hpp"
#include "resources/shader.hpp"
#include "shader-lang/reflection.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/eye-adaptation.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-frame.hpp"
#include "trace.hpp"

namespace astralix {

class PostProcessPass : public FramePass {
public:
  explicit PostProcessPass(
      rendering::ResolvedMeshDraw fullscreen_quad = {},
      EyeAdaptationState *eye_adaptation_state = nullptr
  )
      : m_fullscreen_quad(std::move(fullscreen_quad)),
        m_eye_adaptation_state(eye_adaptation_state) {}
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
    auto *exposure_buffer =
        ctx.find_storage_buffer(std::string(k_eye_adaptation_exposure_resource));

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
        bindings,
        shader_binding_id("__globals.screen_texture"),
        ImageViewRef{.image = scene_color_image}
    );
    frame.add_sampled_image_binding(
        bindings,
        shader_binding_id("__globals.bloom_texture"),
        ImageViewRef{.image = bloom_image}
    );

    if (exposure_buffer != nullptr) {
      const auto *scene_frame = ctx.scene();
      if (scene_frame == nullptr || !scene_frame->eye_adaptation.enabled) {
        const EyeAdaptationExposureData default_exposure{};
        exposure_buffer->set_data(&default_exposure, sizeof(default_exposure));
        if (m_eye_adaptation_state != nullptr) {
          m_eye_adaptation_state->initialized = false;
        }
      }

      frame.add_storage_buffer_binding(
          bindings,
          shader_binding_id("exposure_data"),
          k_eye_adaptation_exposure_binding_point,
          exposure_buffer->renderer_id()
      );
    }

    const auto *scene_frame_ptr = ctx.scene();
    const float bloom_strength = scene_frame_ptr != nullptr
        ? scene_frame_ptr->tonemapping.bloom_strength : 0.12f;
    const float gamma = scene_frame_ptr != nullptr
        ? scene_frame_ptr->tonemapping.gamma : 2.2f;
    const int tonemap_operator = scene_frame_ptr != nullptr
        ? scene_frame_ptr->tonemapping.tonemap_operator : 1;
    frame.add_value_binding(
        bindings,
        shader_binding_id("__globals.bloom_strength"),
        ShaderValueKind::Float,
        bloom_strength
    );
    frame.add_value_binding(
        bindings,
        shader_binding_id("__globals.gamma"),
        ShaderValueKind::Float,
        gamma
    );
    frame.add_value_binding(
        bindings,
        shader_binding_id("__globals.tonemap_operator"),
        ShaderValueKind::Int,
        tonemap_operator
    );

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
  EyeAdaptationState *m_eye_adaptation_state = nullptr;
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
