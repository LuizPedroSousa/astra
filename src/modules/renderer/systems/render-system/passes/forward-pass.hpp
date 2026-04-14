#pragma once

#include "framebuffer.hpp"
#include "log.hpp"
#include "render-pass.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/light-frame.hpp"
#include "systems/render-system/material-binding.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "trace.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class ForwardPass : public FramePass {
public:
  ForwardPass() = default;
  ~ForwardPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("forward_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[ForwardPass] Missing graph dependency: forward_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("ForwardPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr) {
      LOG_WARN("[ForwardPass] Skipping record: scene frame is unavailable");
      return;
    }

    if (m_shader == nullptr) {
      LOG_WARN("[ForwardPass] Skipping record: forward shader is unavailable");
      return;
    }

    if (!scene_frame->main_camera.has_value()) {
      LOG_WARN("[ForwardPass] Skipping record: no main camera selected");
      return;
    }

    if (scene_frame->opaque_surfaces.empty()) {
      LOG_WARN(
          "[ForwardPass] Skipping record: scene has no extracted surfaces"
      );
      return;
    }

    const auto *shadow_map = ctx.find_graph_image("shadow_map");
    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *bloom_extract = ctx.find_graph_image("bloom_extract");
    const auto *entity_pick_resource = ctx.find_graph_image("entity_pick");
    const auto *scene_depth_resource = ctx.find_graph_image("scene_depth");

    if (shadow_map == nullptr || scene_color_resource == nullptr ||
        bloom_extract == nullptr || entity_pick_resource == nullptr ||
        scene_depth_resource == nullptr || m_shader == nullptr) {
      return;
    }

    using namespace shader_bindings::engine_shaders_lighting_forward_axsl;
    const rendering::MaterialBindingLayout engine_material_layout{
        .base_color = rendering::MaterialTextureBindingPoint{
            .logical_name = MaterialResources::base_color.logical_name,
            .binding_id = MaterialResources::base_color.binding_id,
        },
        .normal = rendering::MaterialTextureBindingPoint{
            .logical_name = MaterialResources::normal.logical_name,
            .binding_id = MaterialResources::normal.binding_id,
        },
        .metallic = rendering::MaterialTextureBindingPoint{
            .logical_name = MaterialResources::metallic.logical_name,
            .binding_id = MaterialResources::metallic.binding_id,
        },
        .roughness = rendering::MaterialTextureBindingPoint{
            .logical_name = MaterialResources::roughness.logical_name,
            .binding_id = MaterialResources::roughness.binding_id,
        },
        .occlusion = rendering::MaterialTextureBindingPoint{
            .logical_name = MaterialResources::occlusion.logical_name,
            .binding_id = MaterialResources::occlusion.binding_id,
        },
        .emissive = rendering::MaterialTextureBindingPoint{
            .logical_name = MaterialResources::emissive.logical_name,
            .binding_id = MaterialResources::emissive.binding_id,
        },
        .displacement = rendering::MaterialTextureBindingPoint{
            .logical_name = MaterialResources::displacement.logical_name,
            .binding_id = MaterialResources::displacement.binding_id,
        },
    };

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*scene_color_resource);

    bool rendering_started = false;
    ImageHandle scene_color{};
    ImageHandle bloom{};
    ImageHandle entity_id{};
    ImageHandle scene_depth{};
    ImageHandle shadow_depth{};
    RenderPipelineHandle pipeline{};
    RenderBindingGroupHandle scene_bindings{};
    std::unordered_map<
        rendering::MaterialGroupKey,
        RenderBindingGroupHandle,
        rendering::MaterialGroupKeyHash>
        material_bindings;

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "forward-pass";
    pipeline_desc.depth_stencil.depth_test = true;
    pipeline_desc.depth_stencil.depth_write = true;
    pipeline_desc.depth_stencil.compare_op = CompareOp::Less;
    pipeline_desc.blend_attachments = {
        BlendAttachmentState::replace(),
    };

    const auto ensure_rendering_started = [&] {
      if (rendering_started) {
        return;
      }

      scene_color = ctx.register_graph_image(
          "forward.scene-color", *scene_color_resource
      );
      bloom = ctx.register_graph_image("forward.bloom", *bloom_extract);
      entity_id = ctx.register_graph_image(
          "forward.entity-id", *entity_pick_resource
      );
      scene_depth = ctx.register_graph_image(
          "forward.scene-depth", *scene_depth_resource, ImageAspect::Depth
      );
      shadow_depth = ctx.register_graph_image(
          "forward.shadow-map", *shadow_map, ImageAspect::Depth
      );

      RenderingInfo info;
      info.debug_name = "forward-pass";
      info.extent = extent;
      info.color_attachments.push_back(ColorAttachmentRef{
          .view = ImageViewRef{.image = scene_color},
          .load_op = AttachmentLoadOp::Clear,
          .store_op = AttachmentStoreOp::Store,
          .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
      });
      info.color_attachments.push_back(ColorAttachmentRef{
          .view = ImageViewRef{.image = bloom},
          .load_op = AttachmentLoadOp::Clear,
          .store_op = AttachmentStoreOp::Store,
          .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
      });
      info.color_attachments.push_back(ColorAttachmentRef{
          .view = ImageViewRef{.image = entity_id},
          .load_op = AttachmentLoadOp::Clear,
          .store_op = AttachmentStoreOp::Store,
          .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
      });
      info.depth_stencil_attachment = DepthStencilAttachmentRef{
          .view = ImageViewRef{
              .image = scene_depth,
              .aspect = ImageAspect::Depth,
          },
          .depth_load_op = AttachmentLoadOp::Clear,
          .depth_store_op = AttachmentStoreOp::Store,
          .clear_depth = 1.0f,
          .stencil_load_op = AttachmentLoadOp::DontCare,
          .stencil_store_op = AttachmentStoreOp::DontCare,
          .clear_stencil = 0,
      };

      recorder.begin_rendering(info);
      rendering_started = true;
    };

    const auto ensure_pipeline_and_scene_bindings = [&] {
      ensure_rendering_started();

      if (!pipeline.valid()) {
        pipeline = frame.register_pipeline(pipeline_desc, m_shader);
      }

      if (scene_bindings.valid()) {
        return;
      }

      scene_bindings = frame.register_binding_group(
          make_binding_group_desc(
              "forward-pass.scene",
              "forward-pass",
              m_shader,
              0,
              "forward-pass.scene",
              RenderBindingScope::Pass,
              RenderBindingCachePolicy::Reuse,
              RenderBindingSharing::LocalOnly,
              0,
              RenderBindingStability::FrameLocal
          )
      );
      frame.add_sampled_image_binding(
          scene_bindings,
          SceneLightResources::shadow_map.binding_id,
          ImageViewRef{
              .image = shadow_depth,
              .aspect = ImageAspect::Depth,
          }
      );
      rendering::record_shader_params(frame, scene_bindings, CameraParams{
                                                                .view = scene_frame->main_camera->view,
                                                                .projection = scene_frame->main_camera->projection,
                                                                .position = scene_frame->main_camera->position,
                                                            });
      rendering::record_shader_params(
          frame,
          scene_bindings,
          rendering::build_forward_scene_params(scene_frame->light_frame)
      );
    };

    for (const auto &surface : scene_frame->opaque_surfaces) {
      if (surface.mesh.vertex_array == nullptr ||
          surface.mesh.index_count == 0) {
        continue;
      }

      ensure_pipeline_and_scene_bindings();

      const auto material_key =
          rendering::make_material_group_key(surface.material);
      auto material_it = material_bindings.find(material_key);
      if (material_it == material_bindings.end()) {
        const auto material_group = frame.register_binding_group(
            make_binding_group_desc(
                "forward-pass.material",
                "forward-pass",
                m_shader,
                1,
                "forward-pass.material",
                RenderBindingScope::Material,
                RenderBindingCachePolicy::Reuse,
                RenderBindingSharing::LocalOnly,
                0,
                RenderBindingStability::FrameLocal
            )
        );
        const auto material_binding =
            rendering::record_resolved_material_bindings(
                frame, material_group, surface.material, engine_material_layout
            );
        rendering::record_shader_params(
            frame,
            material_group,
            rendering::build_forward_material_params(material_binding)
        );
        material_it =
            material_bindings.emplace(material_key, material_group).first;
      }

      const auto draw_bindings = frame.register_binding_group(
          make_binding_group_desc(
              "forward-pass.draw",
              "forward-pass",
              m_shader,
              2,
              "forward-pass.draw",
              RenderBindingScope::Draw,
              RenderBindingCachePolicy::Ephemeral,
              RenderBindingSharing::LocalOnly,
              0,
              RenderBindingStability::Transient
          )
      );
      rendering::record_shader_params(
          frame, draw_bindings, DrawParams{
                                    .use_instancing = false,
                                    .g_model = surface.model,
                                    .bloom_enabled = surface.bloom_enabled,
                                    .bloom_layer = surface.bloom_layer,
                                    .entity_id = static_cast<int>(surface.pick_id),
                                }
      );

      const auto mesh_buffer = frame.register_vertex_array(
          "forward-pass.mesh", surface.mesh.vertex_array
      );

      recorder.bind_pipeline(pipeline);
      recorder.bind_binding_group(scene_bindings);
      recorder.bind_binding_group(material_it->second);
      recorder.bind_binding_group(draw_bindings);
      recorder.bind_vertex_buffer(mesh_buffer);
      recorder.bind_index_buffer(mesh_buffer, IndexType::Uint32);
      recorder.draw_indexed(DrawIndexedArgs{
          .index_count = surface.mesh.index_count,
      });
    }

    if (rendering_started) {
      recorder.end_rendering();
    }
  }

  std::string name() const override { return "ForwardPass"; }

private:
  Ref<Shader> m_shader = nullptr;
};

} // namespace astralix
