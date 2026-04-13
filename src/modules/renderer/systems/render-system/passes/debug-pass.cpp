#include "systems/render-system/passes/debug-pass.hpp"

#include "base.hpp"
#include "components/light.hpp"
#include "log.hpp"
#include "managers/scene-manager.hpp"
#include "managers/window-manager.hpp"
#include "systems/render-system/scene-selection.hpp"
#include <array>

namespace astralix {
namespace {

rendering::DirectionalShadowSettings resolve_debug_shadow_settings() {
  const auto *active_scene = SceneManager::get()->get_active_scene();
  if (active_scene == nullptr) {
    return rendering::DirectionalShadowSettings{};
  }

  const auto &world = active_scene->world();
  rendering::DirectionalShadowSettings settings{};
  bool found = false;

  world.each<scene::Transform, rendering::Light>(
      [&](EntityID entity_id, const scene::Transform &, const rendering::Light &light) {
        if (found || !world.active(entity_id) ||
            light.type != rendering::LightType::Directional) {
          return;
        }

        if (const auto *shadow =
                world.get<rendering::DirectionalShadowSettings>(entity_id);
            shadow != nullptr) {
          settings = *shadow;
        }

        found = true;
      }
  );

  return settings;
}

} // namespace

void DebugGBufferPass::setup(PassSetupContext &ctx) {
  m_shader = ctx.require_shader("debug_g_buffer_shader");

  if (m_shader == nullptr) {
    LOG_WARN("[DebugGBufferPass] Missing graph dependency: debug_g_buffer_shader");
    set_enabled(false);
  }
}

void DebugGBufferPass::record(PassRecordContext &ctx, PassRecorder &recorder) {
  ASTRA_PROFILE_N("DebugGBufferPass::record");

  if (input::IS_KEY_RELEASED(input::KeyCode::F4)) {
    m_active = !m_active;
  }

  const auto *scene_color_resource = ctx.find_graph_image("scene_color");
  const auto *g_position_resource = ctx.find_graph_image("g_position");
  const auto *g_normal_resource = ctx.find_graph_image("g_normal");
  const auto *g_albedo_resource = ctx.find_graph_image("g_albedo");
  const auto *g_emissive_resource = ctx.find_graph_image("g_emissive");

  if (!m_active || scene_color_resource == nullptr ||
      g_position_resource == nullptr || g_normal_resource == nullptr ||
      g_albedo_resource == nullptr || g_emissive_resource == nullptr ||
      m_shader == nullptr || m_fullscreen_quad.vertex_array == nullptr ||
      m_fullscreen_quad.index_count == 0) {
    return;
  }
  int requested_attachment_index = m_attachment_index;

  if (input::IS_KEY_RELEASED(input::KeyCode::D1)) {
    requested_attachment_index = 0;
  } else if (input::IS_KEY_RELEASED(input::KeyCode::D2)) {
    requested_attachment_index = 1;
  } else if (input::IS_KEY_RELEASED(input::KeyCode::D3)) {
    requested_attachment_index = 2;
  } else if (input::IS_KEY_RELEASED(input::KeyCode::D4)) {
    requested_attachment_index = 3;
  }

  if (requested_attachment_index >= 0 && requested_attachment_index < 4) {
    m_attachment_index = requested_attachment_index;
  }

  if (m_attachment_index < 0 || m_attachment_index >= 4) {
    m_attachment_index = 0;
  }

  using namespace shader_bindings::engine_shaders_debug_g_buffer_axsl;

  auto &frame = ctx.frame();
  const auto extent = ctx.graph_image_extent(*scene_color_resource);

  const std::array<const RenderGraphResource *, 4> attachments = {
      g_position_resource,
      g_normal_resource,
      g_albedo_resource,
      g_emissive_resource,
  };

  auto selected_attachment = ctx.register_graph_image(
      "debug-gbuffer.attachment",
      *attachments[static_cast<size_t>(m_attachment_index)]
  );
  auto normal_mask = ctx.register_graph_image(
      "debug-gbuffer.normal-mask", *g_normal_resource
  );

  auto scene_color = ctx.register_graph_image(
      "debug-gbuffer.scene-color", *scene_color_resource
  );

  const auto bindings = frame.register_binding_group(
      make_binding_group_desc(
          "debug-gbuffer-pass",
          "debug-gbuffer-pass",
          m_shader,
          0,
          "debug-gbuffer-pass",
          RenderBindingScope::Pass,
          RenderBindingCachePolicy::Reuse,
          RenderBindingSharing::LocalOnly,
          0,
          RenderBindingStability::FrameLocal
      )
  );

  frame.add_sampled_image_binding(
      bindings, GBufferResources::attachment.binding_id,
      ImageViewRef{.image = selected_attachment}
  );
  frame.add_sampled_image_binding(
      bindings, GBufferResources::g_normal_mask.binding_id,
      ImageViewRef{.image = normal_mask}
  );

  rendering::record_shader_params(frame, bindings, GBufferParams{
                                                       .near_plane = 0.1f,
                                                       .far_plane = 100.0f,
                                                   });

  RenderPipelineDesc pipeline_desc;
  pipeline_desc.debug_name = "debug-gbuffer-pass";
  pipeline_desc.depth_stencil.depth_test = false;
  pipeline_desc.depth_stencil.depth_write = false;

  const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
  const auto quad_buffer = frame.register_vertex_array(
      "debug-gbuffer-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
  );

  RenderingInfo info;
  info.debug_name = "debug-gbuffer-pass";
  info.extent = extent;
  info.color_attachments.push_back(ColorAttachmentRef{
      .view = ImageViewRef{.image = scene_color},
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

void DebugOverlayPass::setup(PassSetupContext &ctx) {
  m_depth_shader = ctx.require_shader("debug_depth_shader");

  if (m_depth_shader == nullptr) {
    LOG_WARN("[DebugOverlayPass] Missing graph dependency: debug_depth_shader");
    set_enabled(false);
  }
}

void DebugOverlayPass::record(
    PassRecordContext &ctx, PassRecorder &recorder
) {
  ASTRA_PROFILE_N("DebugOverlayPass::record");

  const auto *scene_color_resource = ctx.find_graph_image("scene_color");
  const auto *shadow_map_resource = ctx.find_graph_image("shadow_map");

  if (input::IS_KEY_RELEASED(input::KeyCode::F2) &&
      shadow_map_resource != nullptr && m_depth_shader != nullptr) {
    if (input::IS_KEY_DOWN(input::KeyCode::LeftShift)) {
      m_depth_fullscreen = !m_depth_fullscreen;
    } else {
      m_depth_active = !m_depth_active;
    }
  }

  if (!m_depth_active || shadow_map_resource == nullptr ||
      m_depth_shader == nullptr || scene_color_resource == nullptr ||
      m_fullscreen_quad.vertex_array == nullptr ||
      m_fullscreen_quad.index_count == 0) {
    return;
  }
  using namespace shader_bindings::engine_shaders_debug_depth_axsl;

  const auto shadow = resolve_debug_shadow_settings();

  auto &frame = ctx.frame();
  const auto extent = ctx.graph_image_extent(*scene_color_resource);

  auto shadow_depth = ctx.register_graph_image(
      "debug-overlay.shadow-depth", *shadow_map_resource, ImageAspect::Depth
  );
  auto scene_color = ctx.register_graph_image(
      "debug-overlay.scene-color", *scene_color_resource
  );

  const auto bindings = frame.register_binding_group(
      make_binding_group_desc(
          "debug-overlay-pass",
          "debug-overlay-pass",
          m_depth_shader,
          0,
          "debug-overlay-pass",
          RenderBindingScope::Pass,
          RenderBindingCachePolicy::Reuse,
          RenderBindingSharing::LocalOnly,
          0,
          RenderBindingStability::FrameLocal
      )
  );

  frame.add_sampled_image_binding(
      bindings, DepthResources::depth_map.binding_id,
      ImageViewRef{.image = shadow_depth, .aspect = ImageAspect::Depth}
  );

  rendering::record_shader_params(frame, bindings, DepthParams{
                                                       .near_plane = shadow.near_plane,
                                                       .far_plane = shadow.far_plane,
                                                       .fullscreen = m_depth_fullscreen ? 1 : 0,
                                                   });

  RenderPipelineDesc pipeline_desc;
  pipeline_desc.debug_name = "debug-overlay-pass";
  pipeline_desc.depth_stencil.depth_test = false;
  pipeline_desc.depth_stencil.depth_write = false;

  const auto pipeline = frame.register_pipeline(pipeline_desc, m_depth_shader);
  const auto quad_buffer = frame.register_vertex_array(
      "debug-overlay-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
  );

  RenderingInfo info;
  info.debug_name = "debug-overlay-pass";
  info.extent = extent;
  info.color_attachments.push_back(ColorAttachmentRef{
      .view = ImageViewRef{.image = scene_color},
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

} // namespace astralix
