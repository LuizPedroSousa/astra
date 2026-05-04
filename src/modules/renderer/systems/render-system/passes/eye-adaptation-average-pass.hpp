#pragma once

#include "log.hpp"
#include "render-pass.hpp"
#include "shader-lang/reflection.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/eye-adaptation.hpp"
#include "trace.hpp"

namespace astralix {

class EyeAdaptationAveragePass : public FramePass {
public:
  explicit EyeAdaptationAveragePass(EyeAdaptationState *state = nullptr)
      : m_state(state) {}
  ~EyeAdaptationAveragePass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("eye_adaptation_average_shader");
    if (m_shader == nullptr) {
      LOG_WARN(
          "[EyeAdaptationAveragePass] Missing graph dependency: "
          "eye_adaptation_average_shader"
      );
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("EyeAdaptationAveragePass::record");

    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->eye_adaptation.enabled) {
      return;
    }

    auto *histogram_buffer =
        ctx.find_storage_buffer(std::string(k_eye_adaptation_histogram_resource));
    auto *exposure_buffer =
        ctx.find_storage_buffer(std::string(k_eye_adaptation_exposure_resource));

    if (m_shader == nullptr || histogram_buffer == nullptr ||
        exposure_buffer == nullptr) {
      return;
    }

    if (m_state != nullptr && !m_state->initialized) {
      const EyeAdaptationExposureData initial_data{};
      exposure_buffer->set_data(&initial_data, sizeof(initial_data));
      m_state->initialized = true;
    }

    auto &frame = ctx.frame();
    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "eye-adaptation-average",
            "eye-adaptation-average-pass",
            m_shader,
            0,
            "eye-adaptation-average-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_storage_buffer_binding(
        bindings,
        shader_binding_id("histogram"),
        k_eye_adaptation_histogram_binding_point,
        histogram_buffer->renderer_id()
    );
    frame.add_storage_buffer_binding(
        bindings,
        shader_binding_id("exposure_data"),
        k_eye_adaptation_exposure_binding_point,
        exposure_buffer->renderer_id()
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
        shader_binding_id("__globals.adaptation_speed_up"),
        ShaderValueKind::Float,
        scene_frame->eye_adaptation.adaptation_speed_up
    );
    frame.add_value_binding(
        bindings,
        shader_binding_id("__globals.adaptation_speed_down"),
        ShaderValueKind::Float,
        scene_frame->eye_adaptation.adaptation_speed_down
    );
    frame.add_value_binding(
        bindings,
        shader_binding_id("__globals.key_value"),
        ShaderValueKind::Float,
        scene_frame->eye_adaptation.key_value
    );
    frame.add_value_binding(
        bindings,
        shader_binding_id("__globals.low_percentile"),
        ShaderValueKind::Float,
        scene_frame->eye_adaptation.low_percentile
    );
    frame.add_value_binding(
        bindings,
        shader_binding_id("__globals.high_percentile"),
        ShaderValueKind::Float,
        scene_frame->eye_adaptation.high_percentile
    );
    frame.add_value_binding(
        bindings,
        shader_binding_id("__globals.delta_time"),
        ShaderValueKind::Float,
        static_cast<float>(ctx.dt)
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "eye-adaptation-average";
    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);

    recorder.bind_compute_pipeline(pipeline);
    recorder.bind_binding_group(bindings);
    recorder.dispatch_compute(1u, 1u, 1u);
    recorder.memory_barrier(MemoryBarrierBit::ShaderStorage);
  }

  bool has_side_effects() const override { return true; }

  std::string name() const override { return "EyeAdaptationAveragePass"; }

private:
  EyeAdaptationState *m_state = nullptr;
  Ref<Shader> m_shader = nullptr;
};

} // namespace astralix
