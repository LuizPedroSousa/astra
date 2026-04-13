#pragma once

#include "framebuffer.hpp"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "log.hpp"
#include "render-pass.hpp"
#include "resources/texture.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-frame.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"
#include <array>
#include <random>
#include <vector>

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class SSAOPass : public FramePass {
public:
  explicit SSAOPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~SSAOPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("ssao_shader");
    m_noise_texture = ctx.require_texture_2d("noise_texture");
    m_ssao_kernel.clear();
    std::uniform_real_distribution<float> random_floats(0.0, 1.0);
    std::default_random_engine generator;

    for (unsigned int i = 0; i < k_kernel_size; ++i) {
      glm::vec3 sample(
          random_floats(generator) * 2.0 - 1.0,
          random_floats(generator) * 2.0 - 1.0,
          random_floats(generator)
      );

      sample = glm::normalize(sample);
      sample *= random_floats(generator);

      float scale = static_cast<float>(i) / static_cast<float>(k_kernel_size);
      scale = lerp(0.1f, 1.0f, scale * scale);
      sample *= scale;
      m_ssao_kernel.push_back(sample);
    }

    if (m_shader == nullptr || m_noise_texture == nullptr) {
      LOG_WARN("[SSAOPass] Missing graph dependencies");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("SSAOPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    const auto *g_position_resource = ctx.find_graph_image("g_position");
    const auto *g_normal_resource = ctx.find_graph_image("g_normal");
    const auto *g_albedo_resource = ctx.find_graph_image("g_albedo");
    const auto *ssao_resource = ctx.find_graph_image("ssao");

    if (g_position_resource == nullptr || g_normal_resource == nullptr ||
        g_albedo_resource == nullptr || ssao_resource == nullptr ||
        m_shader == nullptr ||
        m_noise_texture == nullptr ||
        m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_ssao_axsl;

    auto &frame = ctx.frame();
    const auto &ssao_desc = ssao_resource->get_graph_image()->desc;
    const auto extent = ctx.graph_image_extent(*ssao_resource);

    auto g_position =
        ctx.register_graph_image("ssao.g-position", *g_position_resource);
    auto g_normal =
        ctx.register_graph_image("ssao.g-normal", *g_normal_resource);
    auto g_albedo =
        ctx.register_graph_image("ssao.g-albedo", *g_albedo_resource);
    auto noise_image = frame.register_texture_2d(
        "ssao.noise-texture", m_noise_texture
    );
    auto ssao_color = ctx.register_graph_image("ssao.output", *ssao_resource);

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "ssao-pass",
            "ssao-pass",
            m_shader,
            0,
            "ssao-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings, GBufferResources::g_position.binding_id, ImageViewRef{.image = g_position}
    );
    frame.add_sampled_image_binding(
        bindings, GBufferResources::g_normal.binding_id, ImageViewRef{.image = g_normal}
    );
    frame.add_sampled_image_binding(
        bindings, GBufferResources::g_albedo.binding_id, ImageViewRef{.image = g_albedo}
    );
    frame.add_sampled_image_binding(
        bindings, SSAOResources::noise_texture.binding_id, ImageViewRef{.image = noise_image}
    );

    std::array<glm::vec3, k_kernel_size> kernel{};
    for (size_t i = 0; i < kernel.size() && i < m_ssao_kernel.size(); ++i) {
      kernel[i] = m_ssao_kernel[i];
    }

    rendering::record_shader_params(frame, bindings, GBufferParams{});
    rendering::record_shader_params(frame, bindings, CameraParams{
                                                         .view = scene_frame->main_camera->view,
                                                         .projection = scene_frame->main_camera->projection,
                                                         .position = scene_frame->main_camera->position,
                                                     });
    rendering::record_shader_params(frame, bindings, SSAOParams{
                                                         .samples = kernel,
                                                         .kernel_size = static_cast<int>(k_kernel_size),
                                                         .radius = k_ssao_radius,
                                                         .bias = k_ssao_bias,
                                                         .noise_scale = glm::vec2(static_cast<float>(ssao_desc.width) / 4.0f, static_cast<float>(ssao_desc.height) / 4.0f),
                                                     });

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "ssao-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "ssao-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "ssao-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = ssao_color},
        .load_op = AttachmentLoadOp::Clear,
        .store_op = AttachmentStoreOp::Store,
        .clear_color = {1.0f, 1.0f, 1.0f, 1.0f},
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

  std::string name() const override { return "SSAOPass"; }

private:
  static float lerp(float a, float b, float f) {
    return a + f * (b - a);
  }

  Ref<Shader> m_shader = nullptr;
  Ref<Texture2D> m_noise_texture = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
  static constexpr float k_ssao_radius = 1.5f;
  static constexpr float k_ssao_bias = 0.25f;
  static constexpr uint32_t k_kernel_size = 64;

  std::vector<glm::vec3> m_ssao_kernel;
};

} // namespace astralix
