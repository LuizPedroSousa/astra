#pragma once

#include "framebuffer.hpp"
#include "log.hpp"
#include "render-pass.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/light-frame.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-frame.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

namespace astralix {

class VolumetricFogPass : public FramePass {
public:
  explicit VolumetricFogPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~VolumetricFogPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("volumetric_fog_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[VolumetricFogPass] Missing graph dependency: volumetric_fog_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("VolumetricFogPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    if (!scene_frame->volumetric.enabled) {
      return;
    }

    const auto *g_position_resource = ctx.find_graph_image("g_position");
    const auto *shadow_map_resource = ctx.find_graph_image("shadow_map");
    const auto *volumetric_fog_resource = ctx.find_graph_image("volumetric_fog");

    if (g_position_resource == nullptr || shadow_map_resource == nullptr ||
        volumetric_fog_resource == nullptr || m_shader == nullptr ||
        m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_volumetric_fog_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*volumetric_fog_resource);

    auto g_position = ctx.register_graph_image(
        "volumetric-fog.g-position", *g_position_resource
    );
    auto shadow_depth = ctx.register_graph_image(
        "volumetric-fog.shadow-map", *shadow_map_resource, ImageAspect::Depth
    );
    auto volumetric_output = ctx.register_graph_image(
        "volumetric-fog.output", *volumetric_fog_resource
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "volumetric-fog-pass",
            "volumetric-fog-pass",
            m_shader,
            0,
            "volumetric-fog-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings, VolumetricFogResourcesResources::g_position.binding_id,
        ImageViewRef{.image = g_position}
    );
    frame.add_sampled_image_binding(
        bindings, VolumetricFogResourcesResources::shadow_map.binding_id,
        ImageViewRef{.image = shadow_depth, .aspect = ImageAspect::Depth}
    );

    rendering::record_shader_params(frame, bindings, CameraParams{
        .position = scene_frame->main_camera->position,
    });

    m_elapsed_time += static_cast<float>(ctx.dt);

    const auto &volumetric = scene_frame->volumetric;
    const auto &light_frame = scene_frame->light_frame;

    VolumetricFogParamsParams fog_params{};
    rendering::populate_directional_light_params(light_frame, fog_params);
    rendering::populate_point_light_params(light_frame, fog_params);
    rendering::populate_spot_light_params(light_frame, fog_params);

    if (light_frame.directional.cascades_valid) {
      for (size_t i = 0; i < rendering::k_shadow_cascade_count; ++i) {
        fog_params.cascade_matrices[i] = light_frame.directional.cascade_matrices[i];
        fog_params.cascade_split_depths[i] = light_frame.directional.cascade_split_depths[i];
      }
      fog_params.cascades_enabled = 1;
    } else {
      for (size_t i = 0; i < rendering::k_shadow_cascade_count; ++i) {
        fog_params.cascade_matrices[i] = light_frame.directional.light_space_matrix;
        fog_params.cascade_split_depths[i] = 0.0f;
      }
      fog_params.cascades_enabled = 0;
    }
    fog_params.view_matrix = scene_frame->main_camera->view;

    int point_light_count = 0;
    for (size_t i = 0; i < light_frame.point_lights.size(); ++i) {
      if (light_frame.point_lights[i].valid) {
        ++point_light_count;
      }
    }

    fog_params.point_light_count = point_light_count;
    fog_params.spot_light_active = light_frame.spot.valid;
    fog_params.max_steps = volumetric.max_steps;
    fog_params.density = volumetric.density;
    fog_params.scattering = volumetric.scattering;
    fog_params.max_distance = volumetric.max_distance;
    fog_params.intensity = volumetric.intensity;
    fog_params.frame_index = m_frame_index++;
    fog_params.fog_base_height = volumetric.fog_base_height;
    fog_params.height_falloff_rate = volumetric.height_falloff_rate;
    fog_params.noise_scale = volumetric.noise_scale;
    fog_params.noise_weight = volumetric.noise_weight;
    fog_params.wind_direction = volumetric.wind_direction;
    fog_params.wind_speed = volumetric.wind_speed;
    fog_params.time = m_elapsed_time;

    rendering::record_shader_params(frame, bindings, fog_params);

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "volumetric-fog-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "volumetric-fog-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
    );

    RenderingInfo info;
    info.debug_name = "volumetric-fog-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = volumetric_output},
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

  std::string name() const override { return "VolumetricFogPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
  float m_elapsed_time = 0.0f;
  int m_frame_index = 0;
};

} // namespace astralix
