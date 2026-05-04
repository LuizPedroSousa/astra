#pragma once

#include "log.hpp"
#include "render-pass.hpp"
#include "shader-lang/reflection.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/eye-adaptation.hpp"
#include "trace.hpp"

#include <array>

namespace astralix {

class EyeAdaptationHistogramPass : public FramePass {
public:
  EyeAdaptationHistogramPass() = default;
  ~EyeAdaptationHistogramPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("eye_adaptation_histogram_shader");
    if (m_shader == nullptr) {
      LOG_WARN(
          "[EyeAdaptationHistogramPass] Missing graph dependency: "
          "eye_adaptation_histogram_shader"
      );
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("EyeAdaptationHistogramPass::record");

    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->eye_adaptation.enabled) {
      return;
    }

    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    auto *histogram_buffer =
        ctx.find_storage_buffer(std::string(k_eye_adaptation_histogram_resource));

    if (m_shader == nullptr || scene_color_resource == nullptr ||
        histogram_buffer == nullptr) {
      return;
    }

    const auto extent = ctx.graph_image_extent(*scene_color_resource);
    if (extent.width == 0 || extent.height == 0) {
      return;
    }

    std::array<uint32_t, k_eye_adaptation_histogram_bin_count> cleared_bins{};
    histogram_buffer->set_data(cleared_bins.data(), sizeof(cleared_bins));

    auto &frame = ctx.frame();
    auto scene_color_image = ctx.register_graph_image(
        "eye-adaptation-histogram.scene-color", *scene_color_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "eye-adaptation-histogram",
            "eye-adaptation-histogram-pass",
            m_shader,
            0,
            "eye-adaptation-histogram-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        shader_binding_id("__globals.scene_color"),
        ImageViewRef{.image = scene_color_image}
    );
    frame.add_storage_buffer_binding(
        bindings,
        shader_binding_id("histogram"),
        k_eye_adaptation_histogram_binding_point,
        histogram_buffer->renderer_id()
    );
    frame.add_value_binding(
        bindings,
        shader_binding_id("__globals.min_log_luminance"),
        ShaderValueKind::Float,
        scene_frame->eye_adaptation.min_log_luminance
    );
    frame.add_value_binding(
        bindings,
        shader_binding_id("__globals.max_log_luminance"),
        ShaderValueKind::Float,
        scene_frame->eye_adaptation.max_log_luminance
    );
    frame.add_value_binding(
        bindings,
        shader_binding_id("__globals.image_size"),
        ShaderValueKind::Vec2,
        glm::vec2(
            static_cast<float>(extent.width),
            static_cast<float>(extent.height)
        )
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "eye-adaptation-histogram";
    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);

    recorder.bind_compute_pipeline(pipeline);
    recorder.bind_binding_group(bindings);
    recorder.dispatch_compute(
        (extent.width + 15u) / 16u,
        (extent.height + 15u) / 16u,
        1u
    );
    recorder.memory_barrier(MemoryBarrierBit::ShaderStorage);
  }

  bool has_side_effects() const override { return true; }

  std::string name() const override { return "EyeAdaptationHistogramPass"; }

private:
  Ref<Shader> m_shader = nullptr;
};

} // namespace astralix
