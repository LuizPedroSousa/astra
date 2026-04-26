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

class SkyboxPass : public FramePass {
public:
  explicit SkyboxPass(rendering::ResolvedMeshDraw skybox_cube = {})
      : m_skybox_cube(std::move(skybox_cube)) {}
  ~SkyboxPass() override = default;

  void setup(PassSetupContext &ctx) override {
    (void)ctx;
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("SkyboxPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr) {
      LOG_WARN("[SkyboxPass] Skipping record: scene frame is unavailable");
      return;
    }

    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *scene_depth_resource = ctx.find_graph_image("scene_depth");

    if (scene_color_resource == nullptr) {
      LOG_WARN("[SkyboxPass] Skipping record: scene_color image is not available");
      return;
    }

    if (!scene_frame->main_camera.has_value()) {
      LOG_WARN("[SkyboxPass] Skipping record: no main camera selected");
      return;
    }

    if (!scene_frame->skybox.has_value()) {
      LOG_WARN("[SkyboxPass] Skipping record: no skybox selected");
      return;
    }

    const auto &skybox = *scene_frame->skybox;
    if (skybox.shader == nullptr) {
      LOG_WARN("[SkyboxPass] Skipping record: failed to resolve skybox shader");
      return;
    }

    if (skybox.cubemap == nullptr) {
      LOG_WARN("[SkyboxPass] Skipping record: skybox asset is not available");
      return;
    }

    if (m_skybox_cube.vertex_array == nullptr || m_skybox_cube.index_count == 0) {
      LOG_WARN("[SkyboxPass] Skipping record: skybox mesh is unavailable");
      return;
    }

    using namespace shader_bindings::engine_shaders_skybox_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*scene_color_resource);

    const auto scene_color = ctx.register_graph_image(
        "skybox.scene-color", *scene_color_resource
    );
    const auto scene_depth =
        scene_depth_resource != nullptr
            ? ctx.register_graph_image(
                  "skybox.scene-depth", *scene_depth_resource, ImageAspect::Depth
              )
            : ImageHandle{};
    const auto skybox_map = frame.register_texture_cube(
        "skybox.cubemap", skybox.cubemap
    );
    const auto skybox_cube = frame.register_vertex_array(
        "skybox.mesh", m_skybox_cube.vertex_array
    );

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "skybox-pass";
    pipeline_desc.raster.cull_mode = CullMode::None;
    pipeline_desc.depth_stencil.depth_test = true;
    pipeline_desc.depth_stencil.depth_write = false;
    pipeline_desc.depth_stencil.compare_op = CompareOp::LessEqual;
    pipeline_desc.blend_attachments = {BlendAttachmentState::replace()};
    const auto pipeline = frame.register_pipeline(pipeline_desc, skybox.shader);

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "skybox-pass",
            "skybox-pass",
            skybox.shader,
            0,
            "skybox-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );
    frame.add_sampled_image_binding(
        bindings, EntityResources::skybox_map.binding_id,
        ImageViewRef{.image = skybox_map},
        CompiledSampledImageTarget::TextureCube
    );

    rendering::record_shader_params(frame, bindings, LightParams{
                                                         .view_without_transformation = glm::mat4(glm::mat3(scene_frame->main_camera->view)),
                                                         .projection = scene_frame->main_camera->projection,
                                                     });

    RenderingInfo info;
    info.debug_name = "skybox-pass";
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
          .stencil_load_op = AttachmentLoadOp::Load,
          .stencil_store_op = AttachmentStoreOp::Store,
          .clear_stencil = 0,
      };
    }

    recorder.begin_rendering(info);
    recorder.bind_pipeline(pipeline);
    recorder.bind_binding_group(bindings);
    recorder.bind_vertex_buffer(skybox_cube);
    recorder.bind_index_buffer(skybox_cube, IndexType::Uint32);
    recorder.draw_indexed(DrawIndexedArgs{
        .index_count = m_skybox_cube.index_count,
    });
    recorder.end_rendering();
  }

  std::string name() const override { return "SkyboxPass"; }

private:
  rendering::ResolvedMeshDraw m_skybox_cube{};
};

} // namespace astralix
