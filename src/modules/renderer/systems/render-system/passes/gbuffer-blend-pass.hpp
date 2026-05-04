#pragma once

#include "log.hpp"
#include "render-pass.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/material-binding.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "trace.hpp"

#include <array>

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class GBufferBlendPass : public FramePass {
public:
  GBufferBlendPass() = default;
  ~GBufferBlendPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("gbuffer_blend_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[GBufferBlendPass] Missing graph dependency: gbuffer_blend_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("GBufferBlendPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value() ||
        scene_frame->blend_surfaces.empty()) {
      return;
    }

    const auto *g_albedo = ctx.find_graph_image("g_albedo");
    const auto *scene_depth_resource = ctx.find_graph_image("scene_depth");

    bool missing = false;
    if (!g_albedo) { LOG_WARN("[GBufferBlendPass] Missing: g_albedo"); missing = true; }
    if (!scene_depth_resource) { LOG_WARN("[GBufferBlendPass] Missing: scene_depth"); missing = true; }
    if (!m_shader) { LOG_WARN("[GBufferBlendPass] Missing: gbuffer_blend_shader"); missing = true; }
    if (missing) return;

    using namespace shader_bindings::engine_shaders_g_buffer_blend_axsl;
    const rendering::MaterialBindingLayout material_layout{
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
    const auto extent = ctx.graph_image_extent(*g_albedo);

    ImageHandle albedo_handle = ctx.register_graph_image(
        "gbuffer-blend.g-albedo", *g_albedo
    );
    ImageHandle scene_depth = ctx.register_graph_image(
        "gbuffer-blend.scene-depth", *scene_depth_resource, ImageAspect::Depth
    );

    RenderingInfo info;
    info.debug_name = "gbuffer-blend-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = albedo_handle},
        .load_op = AttachmentLoadOp::Load,
        .store_op = AttachmentStoreOp::Store,
        .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
    });
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

    recorder.begin_rendering(info);

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "gbuffer-blend-pass";
    pipeline_desc.depth_stencil.depth_test = true;
    pipeline_desc.depth_stencil.depth_write = false;
    pipeline_desc.depth_stencil.compare_op = CompareOp::LessEqual;
    pipeline_desc.blend_attachments = {
        BlendAttachmentState::alpha_blend(),
    };

    RenderPipelineHandle pipeline{};
    RenderBindingGroupHandle scene_bindings{};
    std::unordered_map<
        rendering::MaterialGroupKey,
        RenderBindingGroupHandle,
        rendering::MaterialGroupKeyHash>
        material_bindings;

    for (const auto &surface : scene_frame->blend_surfaces) {
      if (surface.mesh.vertex_array == nullptr ||
          surface.mesh.index_count == 0) {
        continue;
      }

      if (!pipeline.valid()) {
        const CullMode cull_mode =
            surface.material.double_sided ? CullMode::None : CullMode::Back;
        pipeline_desc.raster.cull_mode = cull_mode;
        pipeline = frame.register_pipeline(pipeline_desc, m_shader);
      }

      if (!scene_bindings.valid()) {
        scene_bindings = frame.register_binding_group(
            make_binding_group_desc(
                "gbuffer-blend.scene",
                "gbuffer-blend",
                m_shader,
                0,
                "gbuffer-blend.scene",
                RenderBindingScope::Pass,
                RenderBindingCachePolicy::Reuse,
                RenderBindingSharing::LocalOnly,
                0,
                RenderBindingStability::FrameLocal
            )
        );
        rendering::record_shader_params(frame, scene_bindings, CameraParams{
            .view = scene_frame->main_camera->view,
            .projection = scene_frame->main_camera->projection,
            .position = scene_frame->main_camera->position,
        });
      }

      const auto material_key =
          rendering::make_material_group_key(surface.material);
      auto material_it = material_bindings.find(material_key);
      if (material_it == material_bindings.end()) {
        const auto material_group = frame.register_binding_group(
            make_binding_group_desc(
                "gbuffer-blend.material",
                "gbuffer-blend",
                m_shader,
                1,
                "gbuffer-blend.material",
                RenderBindingScope::Material,
                RenderBindingCachePolicy::Reuse,
                RenderBindingSharing::LocalOnly,
                0,
                RenderBindingStability::FrameLocal
            )
        );
        const auto material_binding =
            rendering::record_resolved_material_bindings(
                frame, material_group, surface.material, material_layout
            );
        rendering::record_shader_params(
            frame,
            material_group,
            rendering::build_gbuffer_blend_material_params(material_binding)
        );
        material_it =
            material_bindings.emplace(material_key, material_group).first;
      }

      const auto draw_bindings = frame.register_binding_group(
          make_binding_group_desc(
              "gbuffer-blend.draw",
              "gbuffer-blend",
              m_shader,
              2,
              "gbuffer-blend.draw",
              RenderBindingScope::Draw,
              RenderBindingCachePolicy::Ephemeral,
              RenderBindingSharing::LocalOnly,
              0,
              RenderBindingStability::Transient
          )
      );
      rendering::record_shader_params(
          frame, draw_bindings, DrawParams{
              .g_model = surface.model,
              .entity_id = static_cast<int>(surface.pick_id),
          }
      );

      const auto mesh_buffer = frame.register_vertex_array(
          "gbuffer-blend.mesh", surface.mesh.vertex_array
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

    recorder.end_rendering();
  }

  std::string name() const override { return "GBufferBlendPass"; }

private:
  Ref<Shader> m_shader = nullptr;
};

} // namespace astralix
