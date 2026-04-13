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
#include <functional>

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class GridPass : public FramePass {
public:
  explicit GridPass(rendering::ResolvedMeshDraw grid_quad = {},
                    std::function<bool()> toggle_requested = {})
      : m_grid_quad(std::move(grid_quad)),
        m_toggle_requested(std::move(toggle_requested)) {}
  ~GridPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("grid_shader");
    if (m_shader == nullptr || m_grid_quad.vertex_array == nullptr ||
        m_grid_quad.index_count == 0) {
      return;
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("GridPass::record");
    static constexpr int k_surface_render_mode = 0;
    static constexpr int k_y_axis_render_mode = 1;

    if (m_toggle_requested != nullptr && m_toggle_requested()) {
      m_active = !m_active;
    }

    if (!m_active) {
      return;
    }

    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *scene_depth_resource = ctx.find_graph_image("scene_depth");

    const auto *scene_frame = ctx.scene();
    if (scene_color_resource == nullptr || m_shader == nullptr ||
        m_grid_quad.vertex_array == nullptr || m_grid_quad.index_count == 0 ||
        scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    const auto &camera = *scene_frame->main_camera;
    const auto extent = ctx.graph_image_extent(*scene_color_resource);

    auto &frame = ctx.frame();
    const auto scene_color = ctx.register_graph_image(
        "grid.scene-color", *scene_color_resource
    );
    const auto scene_depth =
        scene_depth_resource != nullptr
            ? ctx.register_graph_image(
                  "grid.scene-depth", *scene_depth_resource, ImageAspect::Depth
              )
            : ImageHandle{};
    const auto grid_quad = frame.register_vertex_array(
        "grid.quad", m_grid_quad.vertex_array
    );

    RenderPipelineDesc surface_pipeline_desc;
    surface_pipeline_desc.debug_name = "grid.surface";
    surface_pipeline_desc.raster.cull_mode = CullMode::None;
    surface_pipeline_desc.depth_stencil.depth_test = true;
    surface_pipeline_desc.depth_stencil.depth_write = false;
    surface_pipeline_desc.depth_stencil.compare_op = CompareOp::LessEqual;
    surface_pipeline_desc.blend_attachments = {
        BlendAttachmentState::alpha_blend()
    };

    RenderPipelineDesc y_axis_pipeline_desc = surface_pipeline_desc;
    y_axis_pipeline_desc.debug_name = "grid.y-axis";
    y_axis_pipeline_desc.depth_stencil.depth_test = false;

    const auto surface_pipeline =
        frame.register_pipeline(surface_pipeline_desc, m_shader);
    const auto y_axis_pipeline =
        frame.register_pipeline(y_axis_pipeline_desc, m_shader);
    const glm::mat4 inverse_view = glm::inverse(camera.view);
    const glm::mat4 inverse_projection = glm::inverse(camera.projection);

    const auto make_bindings = [&](const std::string &debug_name,
                                   int render_mode) {
      const auto bindings = frame.register_binding_group(
          make_binding_group_desc(
              debug_name,
              "grid-pass",
              m_shader,
              0,
              debug_name,
              RenderBindingScope::Pass,
              RenderBindingCachePolicy::Reuse,
              RenderBindingSharing::LocalOnly,
              0,
              RenderBindingStability::FrameLocal
          )
      );
      rendering::record_shader_params(
          frame, bindings,
          shader_bindings::engine_shaders_grid_axsl::GridParams{
              .view = camera.view,
              .projection = camera.projection,
              .inverse_view = inverse_view,
              .inverse_projection = inverse_projection,
              .render_mode = render_mode,
          }
      );
      return bindings;
    };

    const auto surface_bindings =
        make_bindings("grid.surface-bindings", k_surface_render_mode);
    const auto y_axis_bindings =
        make_bindings("grid.y-axis-bindings", k_y_axis_render_mode);

    RenderingInfo info;
    info.debug_name = "grid";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = scene_color},
        .load_op = AttachmentLoadOp::Load,
        .store_op = AttachmentStoreOp::Store,
    });
    if (scene_depth.valid()) {
      info.depth_stencil_attachment = DepthStencilAttachmentRef{
          .view = ImageViewRef{
              .image = scene_depth,
              .aspect = ImageAspect::Depth,
          },
          .depth_load_op = AttachmentLoadOp::Load,
          .depth_store_op = AttachmentStoreOp::Store,
          .clear_depth = 1.0f,
          .stencil_load_op = AttachmentLoadOp::DontCare,
          .stencil_store_op = AttachmentStoreOp::DontCare,
          .clear_stencil = 0,
      };
    }

    recorder.begin_rendering(info);
    recorder.bind_vertex_buffer(grid_quad);
    recorder.bind_index_buffer(grid_quad, IndexType::Uint32);

    recorder.bind_pipeline(surface_pipeline);
    recorder.bind_binding_group(surface_bindings);
    recorder.draw_indexed(DrawIndexedArgs{
        .index_count = m_grid_quad.index_count,
    });

    recorder.bind_pipeline(y_axis_pipeline);
    recorder.bind_binding_group(y_axis_bindings);
    recorder.draw_indexed(DrawIndexedArgs{
        .index_count = m_grid_quad.index_count,
    });

    recorder.end_rendering();
  }

  std::string name() const override { return "GridPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_grid_quad{};
  bool m_active = true;
  std::function<bool()> m_toggle_requested;
};

} // namespace astralix
