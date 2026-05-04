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
#include "targets/render-target.hpp"
#include "trace.hpp"

#include <array>
#include <cstddef>

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class GeometryPass : public FramePass {
public:
  GeometryPass() = default;
  ~GeometryPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("geometry_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[GeometryPass] Missing graph dependency: geometry_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("GeometryPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value() ||
        scene_frame->opaque_surfaces.empty()) {
      return;
    }

    const auto *g_position = ctx.find_graph_image("g_position");
    const auto *g_normal = ctx.find_graph_image("g_normal");
    const auto *g_geometric_normal = ctx.find_graph_image("g_geometric_normal");
    const auto *g_albedo = ctx.find_graph_image("g_albedo");
    const auto *g_emissive = ctx.find_graph_image("g_emissive");
    const auto *g_entity_id = ctx.find_graph_image("g_entity_id");
    const auto *g_velocity = ctx.find_graph_image("g_velocity");
    const auto *scene_depth_resource = ctx.find_graph_image("scene_depth");

#define GEOMETRY_PASS_REQUIRED_RESOURCES(ENTRY) \
    ENTRY(g_position)                           \
    ENTRY(g_normal)                             \
    ENTRY(g_geometric_normal)                   \
    ENTRY(g_albedo)                             \
    ENTRY(g_emissive)                           \
    ENTRY(g_entity_id)                          \
    ENTRY(g_velocity)                           \
    ENTRY(scene_depth_resource)

#define CHECK_MISSING(var)                      \
  if (!(var)) {                                 \
    LOG_WARN("[GeometryPass] Missing: " #var);  \
    missing = true;                             \
  }

    bool missing = false;
    GEOMETRY_PASS_REQUIRED_RESOURCES(CHECK_MISSING)
    if (!m_shader) { LOG_WARN("[GeometryPass] Missing: geometry_shader"); missing = true; }
    if (missing) return;

#undef CHECK_MISSING
#undef GEOMETRY_PASS_REQUIRED_RESOURCES

    using namespace shader_bindings::engine_shaders_g_buffer_axsl;
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
    const auto extent = ctx.graph_image_extent(*g_position);

    bool rendering_started = false;
    std::array<ImageHandle, 7> gbuffer_colors{};
    ImageHandle scene_depth{};
    std::array<RenderPipelineHandle, 3> pipelines{};
    RenderBindingGroupHandle scene_bindings{};
    std::unordered_map<
        rendering::MaterialGroupKey,
        RenderBindingGroupHandle,
        rendering::MaterialGroupKeyHash>
        material_bindings;

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "geometry-pass";
    pipeline_desc.depth_stencil.depth_test = true;
    pipeline_desc.depth_stencil.depth_write = true;
    pipeline_desc.depth_stencil.compare_op = CompareOp::Less;

    const auto ensure_rendering_started = [&] {
      if (rendering_started) {
        return;
      }

      gbuffer_colors[0] = ctx.register_graph_image(
          "geometry.g-position", *g_position
      );
      gbuffer_colors[1] = ctx.register_graph_image(
          "geometry.g-normal", *g_normal
      );
      gbuffer_colors[2] = ctx.register_graph_image(
          "geometry.g-geometric-normal", *g_geometric_normal
      );
      gbuffer_colors[3] = ctx.register_graph_image(
          "geometry.g-albedo", *g_albedo
      );
      gbuffer_colors[4] = ctx.register_graph_image(
          "geometry.g-emissive", *g_emissive
      );
      gbuffer_colors[5] = ctx.register_graph_image(
          "geometry.g-entity-id", *g_entity_id
      );
      gbuffer_colors[6] = ctx.register_graph_image(
          "geometry.g-velocity", *g_velocity
      );
      scene_depth = ctx.register_graph_image(
          "geometry.scene-depth", *scene_depth_resource, ImageAspect::Depth
      );

      RenderingInfo info;
      info.debug_name = "geometry-pass";
      info.extent = extent;
      for (ImageHandle attachment : gbuffer_colors) {
        info.color_attachments.push_back(ColorAttachmentRef{
            .view = ImageViewRef{.image = attachment},
            .load_op = AttachmentLoadOp::Clear,
            .store_op = AttachmentStoreOp::Store,
            .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
        });
      }
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

    const auto ensure_pipeline_and_scene_bindings = [&](
                                                    CullMode cull_mode) -> RenderPipelineHandle {
      ensure_rendering_started();

      const size_t pipeline_index = static_cast<size_t>(cull_mode);
      if (!pipelines[pipeline_index].valid()) {
        pipeline_desc.raster.cull_mode = cull_mode;
        pipelines[pipeline_index] = frame.register_pipeline(pipeline_desc, m_shader);
      }

      if (scene_bindings.valid()) {
        return pipelines[pipeline_index];
      }

      scene_bindings = frame.register_binding_group(
          make_binding_group_desc(
              "geometry-pass.scene",
              "geometry-pass",
              m_shader,
              0,
              "geometry-pass.scene",
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
          .previous_view = scene_frame->main_camera->previous_view,
          .previous_projection =
              scene_frame->main_camera->previous_projection,
      });
      rendering::record_shader_params(
          frame,
          scene_bindings,
          rendering::build_gbuffer_scene_params(scene_frame->light_frame)
      );

      return pipelines[pipeline_index];
    };

    for (const auto &surface : scene_frame->opaque_surfaces) {
      if (surface.mesh.vertex_array == nullptr ||
          surface.mesh.index_count == 0) {
        continue;
      }

      const CullMode cull_mode =
          surface.material.double_sided ? CullMode::None : CullMode::Back;
      const auto pipeline =
          ensure_pipeline_and_scene_bindings(cull_mode);

      const auto material_key =
          rendering::make_material_group_key(surface.material);
      auto material_it = material_bindings.find(material_key);
      if (material_it == material_bindings.end()) {
        const auto material_group = frame.register_binding_group(
            make_binding_group_desc(
                "geometry-pass.material",
                "geometry-pass",
                m_shader,
                1,
                "geometry-pass.material",
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
            rendering::build_gbuffer_material_params(material_binding)
        );
        material_it =
            material_bindings.emplace(material_key, material_group).first;
      }

      const auto draw_bindings = frame.register_binding_group(
          make_binding_group_desc(
              "geometry-pass.draw",
              "geometry-pass",
              m_shader,
              2,
              "geometry-pass.draw",
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
              .previous_model = surface.previous_model,
              .has_previous_model = surface.has_previous_model,
              .bloom_enabled = surface.bloom_enabled,
              .bloom_layer = surface.bloom_layer,
              .entity_id = static_cast<int>(surface.pick_id),
          }
      );

      const auto mesh_buffer = frame.register_vertex_array(
          "geometry-pass.mesh", surface.mesh.vertex_array
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

  std::string name() const override { return "GeometryPass"; }

private:
  Ref<Shader> m_shader = nullptr;
};

} // namespace astralix
