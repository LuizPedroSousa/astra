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

class VolumetricCompositePass : public FramePass {
public:
  explicit VolumetricCompositePass(
      rendering::ResolvedMeshDraw fullscreen_quad = {}
  )
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~VolumetricCompositePass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("volumetric_composite_shader");

    if (m_shader == nullptr) {
      LOG_WARN(
          "[VolumetricCompositePass] Missing graph dependency: "
          "volumetric_composite_shader"
      );
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("VolumetricCompositePass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    if (!scene_frame->volumetric.enabled) {
      return;
    }

    const auto *volumetric_resource = ctx.find_graph_image("volumetric_temporal");
    if (volumetric_resource == nullptr) {
      volumetric_resource = ctx.find_graph_image("volumetric_blur");
    }
    if (volumetric_resource == nullptr) {
      volumetric_resource = ctx.find_graph_image("volumetric_fog");
    }
    const auto *scene_color_resource = ctx.find_graph_image("scene_color");

    if (volumetric_resource == nullptr || scene_color_resource == nullptr ||
        m_shader == nullptr || m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_volumetric_composite_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*scene_color_resource);

    auto volumetric_input = ctx.register_graph_image(
        "volumetric-composite.volumetric-input", *volumetric_resource
    );
    auto scene_color_target = ctx.register_graph_image(
        "volumetric-composite.scene-color-target", *scene_color_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "volumetric-composite-pass",
            "volumetric-composite-pass",
            m_shader,
            0,
            "volumetric-composite-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings,
        VolumetricCompositeResourcesResources::volumetric_input.binding_id,
        ImageViewRef{.image = volumetric_input}
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "volumetric-composite-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;
    pipeline_desc.blend_attachments = {{
        .enabled = true,
        .src = BlendFactor::One,
        .dst = BlendFactor::SrcAlpha,
    }};

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "volumetric-composite-pass.fullscreen-quad",
        m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "volumetric-composite-pass";
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

  std::string name() const override { return "VolumetricCompositePass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
