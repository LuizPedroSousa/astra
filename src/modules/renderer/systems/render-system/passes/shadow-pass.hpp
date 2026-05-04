#pragma once

#include "framebuffer.hpp"
#include "log.hpp"
#include "render-pass.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-frame.hpp"
#include "trace.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class ShadowPass : public FramePass {
public:
  ShadowPass() = default;
  ~ShadowPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("shadow_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[ShadowPass] Missing graph dependency: shadow_shader");
      set_enabled(false);
      return;
    }

    if (m_shader->descriptor_id() != "shaders::shadow_map") {
      LOG_WARN("[ShadowPass] Wrong shader dependency wired into graph");
      set_enabled(false);
      return;
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("ShadowPass::record");

    const auto *shadow_map_resource = ctx.find_graph_image("shadow_map");
    if (shadow_map_resource == nullptr) {
      LOG_WARN("[ShadowPass] shadow_map resource not found");
      return;
    }

    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || scene_frame->shadow_draws.empty() ||
        !scene_frame->light_frame.directional.valid || m_shader == nullptr) {
      LOG_WARN("[ShadowPass] early exit: scene=", scene_frame != nullptr, " draws=", scene_frame ? scene_frame->shadow_draws.size() : 0, " light_valid=", scene_frame ? scene_frame->light_frame.directional.valid : false, " shader=", m_shader != nullptr);
      return;
    }

    const auto extent = ctx.graph_image_extent(*shadow_map_resource);

    auto &frame = ctx.frame();
    const auto shadow_depth = ctx.register_graph_image(
        "shadow-pass.depth", *shadow_map_resource, ImageAspect::Depth
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "shadow-pass";
    pipeline_desc.depth_format =
        shadow_map_resource->get_graph_image()->desc.format;
    pipeline_desc.raster.cull_mode = CullMode::None;
    pipeline_desc.raster.depth_bias.enabled = true;
    pipeline_desc.raster.depth_bias.constant_factor = 1.0f;
    pipeline_desc.raster.depth_bias.slope_factor = 1.75f;
    pipeline_desc.depth_stencil.depth_test = true;
    pipeline_desc.depth_stencil.depth_write = true;
    pipeline_desc.depth_stencil.compare_op = CompareOp::Less;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);

    RenderingInfo info;
    info.debug_name = "shadow-pass";
    info.extent = extent;
    info.depth_stencil_attachment = DepthStencilAttachmentRef{
        .view = ImageViewRef{.image = shadow_depth, .aspect = ImageAspect::Depth},
        .depth_load_op = AttachmentLoadOp::Clear,
        .depth_store_op = AttachmentStoreOp::Store,
        .clear_depth = 1.0f,
        .stencil_load_op = AttachmentLoadOp::DontCare,
        .stencil_store_op = AttachmentStoreOp::DontCare,
        .clear_stencil = 0,
    };

    recorder.begin_rendering(info);
    recorder.bind_pipeline(pipeline);

    const auto &directional = scene_frame->light_frame.directional;
    const bool use_cascades = directional.cascades_valid;
    const size_t cascade_count = use_cascades ? rendering::k_shadow_cascade_count : 1u;

    uint32_t cascade_width = extent.width;
    uint32_t cascade_height = extent.height;
    if (use_cascades) {
      cascade_width = extent.width / 2;
      cascade_height = extent.height / 2;
    }

    for (size_t cascade_index = 0; cascade_index < cascade_count; ++cascade_index) {
      glm::mat4 light_space_matrix = use_cascades
                                         ? directional.cascade_matrices[cascade_index]
                                         : directional.light_space_matrix;

      if (use_cascades) {
        uint32_t offset_x = static_cast<uint32_t>(cascade_index % 2) * cascade_width;
        uint32_t offset_y = static_cast<uint32_t>(cascade_index / 2) * cascade_height;
        recorder.set_viewport(offset_x, offset_y, cascade_width, cascade_height);
        recorder.set_scissor(true, offset_x, offset_y, cascade_width, cascade_height);
      }

      const auto scene_bindings = frame.register_binding_group(
          make_binding_group_desc(
              "shadow-pass.scene",
              "shadow-pass",
              m_shader,
              0,
              "shadow-pass.scene." + std::to_string(cascade_index),
              RenderBindingScope::Pass,
              RenderBindingCachePolicy::Ephemeral,
              RenderBindingSharing::LocalOnly,
              0,
              RenderBindingStability::Transient
          )
      );
      rendering::record_shader_params(
          frame,
          scene_bindings,
          shader_bindings::engine_shaders_shadow_map_axsl::ShadowPassParams{
              .light_space_matrix = light_space_matrix,
          }
      );
      recorder.bind_binding_group(scene_bindings);

      for (const auto &shadow_draw : scene_frame->shadow_draws) {
        if (shadow_draw.mesh.vertex_array == nullptr ||
            shadow_draw.mesh.index_count == 0) {
          continue;
        }

        const auto bindings = frame.register_binding_group(
            make_binding_group_desc(
                "shadow-pass.draw",
                "shadow-pass",
                m_shader,
                1,
                "shadow-pass.draw",
                RenderBindingScope::Draw,
                RenderBindingCachePolicy::Ephemeral,
                RenderBindingSharing::LocalOnly,
                0,
                RenderBindingStability::Transient
            )
        );
        rendering::record_shader_params(
            frame,
            bindings,
            shader_bindings::engine_shaders_shadow_map_axsl::ShadowDrawParams{
                .g_model = shadow_draw.model,
            }
        );

        const auto mesh_buffer = frame.register_vertex_array(
            "shadow-pass.mesh", shadow_draw.mesh.vertex_array
        );

        recorder.bind_binding_group(bindings);
        recorder.bind_vertex_buffer(mesh_buffer);
        recorder.bind_index_buffer(mesh_buffer, IndexType::Uint32);
        recorder.draw_indexed(DrawIndexedArgs{
            .index_count = shadow_draw.mesh.index_count,
        });
      }
    }

    if (use_cascades) {
      recorder.set_scissor(false);
    }

    recorder.end_rendering();

    if (m_debug_frame_counter++ % 300 == 0) {
      LOG_DEBUG("[ShadowPass] rendered: cascades=", use_cascades, " cascade_count=", cascade_count, " shadow_draws=", scene_frame->shadow_draws.size(), " extent=", extent.width, "x", extent.height);
    }
  }

  std::string name() const override { return "ShadowPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  uint32_t m_debug_frame_counter = 0;
};

} // namespace astralix
