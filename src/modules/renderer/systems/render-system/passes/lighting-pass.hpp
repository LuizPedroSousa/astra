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

class LightingPass : public FramePass {
public:
  explicit LightingPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~LightingPass() override = default;

  void setup(PassSetupContext &ctx) override {
    m_shader = ctx.require_shader("lighting_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[LightingPass] Missing graph dependency: lighting_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("LightingPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    const auto *shadow_map = ctx.find_graph_image("shadow_map");
    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *bloom_extract = ctx.find_graph_image("bloom_extract");
    const auto *entity_pick = ctx.find_graph_image("entity_pick");
    const auto *g_position_resource = ctx.find_graph_image("g_position");
    const auto *g_normal_resource = ctx.find_graph_image("g_normal");
    const auto *g_albedo_resource = ctx.find_graph_image("g_albedo");
    const auto *g_emissive_resource = ctx.find_graph_image("g_emissive");
    const auto *g_entity_id_resource = ctx.find_graph_image("g_entity_id");
    const auto *ssao_resource = ctx.find_graph_image("ssao_blur");

    if (shadow_map == nullptr || scene_color_resource == nullptr ||
        bloom_extract == nullptr || entity_pick == nullptr ||
        g_position_resource == nullptr || g_normal_resource == nullptr ||
        g_albedo_resource == nullptr || g_emissive_resource == nullptr ||
        g_entity_id_resource == nullptr || ssao_resource == nullptr ||
        m_shader == nullptr ||
        m_fullscreen_quad.vertex_array == nullptr ||
        m_fullscreen_quad.index_count == 0) {
      return;
    }

    using namespace shader_bindings::engine_shaders_light_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*scene_color_resource);

    auto shadow_depth = ctx.register_graph_image(
        "lighting.shadow-map", *shadow_map, ImageAspect::Depth
    );
    auto g_position = ctx.register_graph_image(
        "lighting.g-position", *g_position_resource
    );
    auto g_normal =
        ctx.register_graph_image("lighting.g-normal", *g_normal_resource);
    auto g_albedo =
        ctx.register_graph_image("lighting.g-albedo", *g_albedo_resource);
    auto g_emissive = ctx.register_graph_image(
        "lighting.g-emissive", *g_emissive_resource
    );
    auto g_entity_id = ctx.register_graph_image(
        "lighting.g-entity-id", *g_entity_id_resource
    );
    auto ssao_blur =
        ctx.register_graph_image("lighting.ssao-blur", *ssao_resource);
    auto scene_color = ctx.register_graph_image(
        "lighting.scene-color", *scene_color_resource
    );
    auto bright_color = ctx.register_graph_image(
        "lighting.scene-bright", *bloom_extract
    );
    auto entity_id = ctx.register_graph_image(
        "lighting.scene-entity-id", *entity_pick
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "lighting-pass",
            "lighting-pass",
            m_shader,
            0,
            "lighting-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    frame.add_sampled_image_binding(
        bindings, LightResources::shadow_map.binding_id,
        ImageViewRef{.image = shadow_depth, .aspect = ImageAspect::Depth});
    frame.add_sampled_image_binding(
        bindings, LightResources::g_position.binding_id,
        ImageViewRef{.image = g_position});
    frame.add_sampled_image_binding(
        bindings, LightResources::g_normal.binding_id,
        ImageViewRef{.image = g_normal});
    frame.add_sampled_image_binding(
        bindings, LightResources::g_albedo.binding_id,
        ImageViewRef{.image = g_albedo});
    frame.add_sampled_image_binding(
        bindings, LightResources::g_emissive.binding_id,
        ImageViewRef{.image = g_emissive});
    frame.add_sampled_image_binding(
        bindings, LightResources::g_entity_id.binding_id,
        ImageViewRef{.image = g_entity_id});
    frame.add_sampled_image_binding(
        bindings, LightResources::g_ssao.binding_id,
        ImageViewRef{.image = ssao_blur});

    rendering::record_shader_params(
        frame, bindings,
        rendering::build_deferred_light_params(scene_frame->light_frame));
    rendering::record_shader_params(frame, bindings, CameraParams{
        .position = scene_frame->main_camera->position,
    });

    RenderPipelineDesc pipeline_desc;
    pipeline_desc.debug_name = "lighting-pass";
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;

    const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
    const auto quad_buffer = frame.register_vertex_array(
        "lighting-pass.fullscreen-quad", m_fullscreen_quad.vertex_array);

    RenderingInfo info;
    info.debug_name = "lighting-pass";
    info.extent = extent;
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = scene_color},
        .load_op = AttachmentLoadOp::Clear,
        .store_op = AttachmentStoreOp::Store,
        .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
    });
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = bright_color},
        .load_op = AttachmentLoadOp::Clear,
        .store_op = AttachmentStoreOp::Store,
        .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
    });
    info.color_attachments.push_back(ColorAttachmentRef{
        .view = ImageViewRef{.image = entity_id},
        .load_op = AttachmentLoadOp::Clear,
        .store_op = AttachmentStoreOp::Store,
        .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
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

  std::string name() const override { return "LightingPass"; }

private:
  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
};

} // namespace astralix
