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

class VolumetricBlurPass : public FramePass {
public:
  enum class Direction { Horizontal, Vertical };

  explicit VolumetricBlurPass(
      Direction direction,
      rendering::ResolvedMeshDraw fullscreen_quad = {}
  )
      : m_direction(direction),
        m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~VolumetricBlurPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("volumetric_blur_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[VolumetricBlurPass] Missing graph dependency: volumetric_blur_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("VolumetricBlurPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->volumetric.enabled) {
      return;
    }

    const std::string input_name = m_direction == Direction::Horizontal
        ? "volumetric_fog"
        : "volumetric_blur_h";
    const std::string output_name = m_direction == Direction::Horizontal
        ? "volumetric_blur_h"
        : "volumetric_blur";

    const auto *input_resource = ctx.find_graph_image(input_name);
    const auto *output_resource = ctx.find_graph_image(output_name);
    const auto *g_position_resource = ctx.find_graph_image("g_position");

    if (input_resource == nullptr || output_resource == nullptr ||
        g_position_resource == nullptr || m_shader == nullptr ||
        m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_volumetric_blur_axsl;

    auto &frame = ctx.frame();
    const auto &output_desc = output_resource->get_graph_image()->desc;
    const auto extent = ctx.graph_image_extent(*output_resource);

    const std::string prefix = m_direction == Direction::Horizontal
        ? "volumetric-blur-h"
        : "volumetric-blur-v";

    auto volumetric_input = ctx.register_graph_image(
        prefix + ".input", *input_resource
    );
    auto g_position = ctx.register_graph_image(
        prefix + ".g-position", *g_position_resource
    );
    auto blur_output = ctx.register_graph_image(
        prefix + ".output", *output_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            prefix + "-pass",
            prefix + "-pass",
            m_shader,
            0,
            prefix + "-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        VolumetricBlurResourcesResources::volumetric_input.binding_id,
        ImageViewRef{.image = volumetric_input}
    );
    frame.add_sampled_image_binding(
        bindings,
        VolumetricBlurResourcesResources::g_position.binding_id,
        ImageViewRef{.image = g_position}
    );

    glm::vec2 direction_vector = m_direction == Direction::Horizontal
        ? glm::vec2(1.0f, 0.0f)
        : glm::vec2(0.0f, 1.0f);

    rendering::record_shader_params(frame, bindings, VolumetricBlurParamsParams{
        .texel_size = glm::vec2(
            1.0f / static_cast<float>(output_desc.width),
            1.0f / static_cast<float>(output_desc.height)),
        .direction = direction_vector,
        .depth_sharpness = 12.0f,
        .kernel_radius = 4,
    });

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = prefix + "-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        prefix + "-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = prefix + "-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = blur_output},
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

  std::string name() const override {
    return m_direction == Direction::Horizontal
        ? "VolumetricBlurHPass"
        : "VolumetricBlurVPass";
  }

private:
  Direction m_direction;
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
