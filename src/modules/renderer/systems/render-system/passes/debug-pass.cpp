#include "systems/render-system/passes/debug-pass.hpp"

#include "base.hpp"
#include "trace.hpp"
#include "components/light.hpp"
#include "log.hpp"
#include "managers/scene-manager.hpp"
#include "renderer-api.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/scene-selection.hpp"

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix {
namespace {

Ref<Shader> load_shader(Ref<RenderTarget> render_target,
                        const ResourceDescriptorID &shader_id) {
  resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
      render_target->renderer_api()->get_backend(), {shader_id});

  return resource_manager()->get_by_descriptor_id<Shader>(shader_id);
}

rendering::DirectionalShadowSettings resolve_debug_shadow_settings() {
  const auto *active_scene = SceneManager::get()->get_active_scene();
  if (active_scene == nullptr) {
    return rendering::DirectionalShadowSettings{};
  }

  const auto &world = active_scene->world();
  rendering::DirectionalShadowSettings settings{};
  bool found = false;

  world.each<scene::Transform, rendering::Light>(
      [&](EntityID entity_id, const scene::Transform &,
          const rendering::Light &light) {
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
      });

  return settings;
}

void release_quad(DebugDepthState &state) {
  state.shader.reset();
  state.mesh.vertex_array.reset();
}

void release_quad(DebugGBufferState &state) {
  state.shader.reset();
  state.mesh.vertex_array.reset();
}

} // namespace

void DebugGBufferPass::setup(
    Ref<RenderTarget> render_target,
    const std::vector<const RenderGraphResource *> &resources) {
  m_render_target = render_target;
  m_scene_color = nullptr;
  m_g_buffer = nullptr;
  m_debug_gbuffer = {};
  set_enabled(true);

  for (auto resource : resources) {
    switch (resource->desc.type) {
      case RenderGraphResourceType::Framebuffer:
        if (resource->desc.name == "scene_color") {
          m_scene_color = resource->get_framebuffer();
        }

        if (resource->desc.name == "g_buffer") {
          m_g_buffer = resource->get_framebuffer();
        }
        break;

      default:
        break;
    }
  }

  if (m_scene_color == nullptr || m_g_buffer == nullptr) {
    set_enabled(false);
    return;
  }

  rendering::ensure_mesh_uploaded(m_debug_gbuffer.mesh, m_render_target);
  m_debug_gbuffer.shader =
      load_shader(m_render_target, "shaders::debug_g_buffer");

  if (m_debug_gbuffer.shader == nullptr) {
    set_enabled(false);
  }
}

void DebugGBufferPass::begin(double) {}

void DebugGBufferPass::execute(double) {
  ASTRA_PROFILE_N("DebugGBufferPass");
  if (input::IS_KEY_RELEASED(input::KeyCode::F4)) {
    m_debug_gbuffer.active = !m_debug_gbuffer.active;
  }

  if (!m_debug_gbuffer.active || m_scene_color == nullptr ||
      m_g_buffer == nullptr || m_debug_gbuffer.shader == nullptr) {
    return;
  }

  m_scene_color->bind();
  draw_gbuffer();
  m_scene_color->unbind();
}

void DebugGBufferPass::end(double) {}

void DebugGBufferPass::cleanup() {
  release_quad(m_debug_gbuffer);
  m_scene_color = nullptr;
  m_g_buffer = nullptr;
}

void DebugGBufferPass::draw_gbuffer() {
  const auto &color_attachments = m_g_buffer->get_color_attachments();
  if (color_attachments.empty()) {
    return;
  }

  int requested_attachment_index = m_debug_gbuffer.attachment_index;

  if (input::IS_KEY_RELEASED(input::KeyCode::D1)) {
    requested_attachment_index = 0;
  } else if (input::IS_KEY_RELEASED(input::KeyCode::D2)) {
    requested_attachment_index = 1;
  } else if (input::IS_KEY_RELEASED(input::KeyCode::D3)) {
    requested_attachment_index = 2;
  } else if (input::IS_KEY_RELEASED(input::KeyCode::D4)) {
    requested_attachment_index = 3;
  }

  if (requested_attachment_index >= 0 &&
      requested_attachment_index < static_cast<int>(color_attachments.size())) {
    m_debug_gbuffer.attachment_index = requested_attachment_index;
  } else if (requested_attachment_index != m_debug_gbuffer.attachment_index) {
    LOG_WARN("Ignoring invalid G-buffer attachment index ",
             requested_attachment_index,
             ". Available color attachments: ", color_attachments.size());
  }

  if (m_debug_gbuffer.attachment_index < 0 ||
      m_debug_gbuffer.attachment_index >=
          static_cast<int>(color_attachments.size())) {
    m_debug_gbuffer.attachment_index = 0;
  }

  auto renderer_api = m_render_target->renderer_api();
  renderer_api->bind_texture_2d(
      color_attachments[m_debug_gbuffer.attachment_index], 0);

  const uint32_t g_normal_mask_attachment =
      color_attachments.size() > 1u
          ? color_attachments[1]
          : color_attachments[m_debug_gbuffer.attachment_index];

  renderer_api->bind_texture_2d(g_normal_mask_attachment, 1);

  auto shader = m_debug_gbuffer.shader;
  shader->bind();

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
  using namespace shader_bindings::engine_shaders_debug_g_buffer_axsl;
  shader->set_all(GBufferParams{
      .attachment = 0,
      .g_normal_mask = 1,
      .near_plane = 0.1f,
      .far_plane = 100.0f,
  });
#endif

  renderer_api->disable_depth_test();
  renderer_api->disable_depth_write();
  renderer_api->draw_indexed(m_debug_gbuffer.mesh.vertex_array,
                             m_debug_gbuffer.mesh.draw_type);
  renderer_api->enable_depth_write();
  renderer_api->enable_depth_test();

  shader->unbind();
}

void DebugOverlayPass::setup(
    Ref<RenderTarget> render_target,
    const std::vector<const RenderGraphResource *> &resources) {
  m_render_target = render_target;
  m_scene_color = nullptr;
  m_shadow_map = nullptr;
  m_debug_depth = {};
  m_debug_normal = {};
  set_enabled(true);

  for (auto resource : resources) {
    switch (resource->desc.type) {
      case RenderGraphResourceType::Framebuffer:
        if (resource->desc.name == "shadow_map") {
          m_shadow_map = resource->get_framebuffer();
        }

        if (resource->desc.name == "scene_color") {
          m_scene_color = resource->get_framebuffer();
        }
        break;

      default:
        break;
    }
  }

  if (m_scene_color == nullptr) {
    set_enabled(false);
    return;
  }

  Shader::create("debug_normal", "shaders/fragment/debug_normal.glsl"_engine,
                 "shaders/vertex/debug_normal.glsl"_engine,
                 "shaders/geometry/debug_normal.glsl"_engine);

  m_debug_normal.shader = load_shader(m_render_target, "debug_normal");

  if (m_shadow_map != nullptr) {
    rendering::ensure_mesh_uploaded(m_debug_depth.mesh, m_render_target);
    m_debug_depth.shader = load_shader(m_render_target, "shaders::debug_depth");

    if (m_debug_depth.shader != nullptr) {
      m_debug_depth.shader->bind();
#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
      using namespace shader_bindings::engine_shaders_debug_depth_axsl;
      m_debug_depth.shader->set(DepthUniform::depth_map, 0);
#else
      m_debug_depth.shader->set_int("depth.depth_map", 0);
#endif
      m_debug_depth.shader->unbind();
    }
  }

  if (m_debug_normal.shader == nullptr &&
      (m_shadow_map == nullptr || m_debug_depth.shader == nullptr)) {
    set_enabled(false);
  }
}

void DebugOverlayPass::begin(double) {}

void DebugOverlayPass::execute(double) {
  ASTRA_PROFILE_N("DebugOverlayPass");
  if (input::IS_KEY_RELEASED(input::KeyCode::F2) && m_shadow_map != nullptr &&
      m_debug_depth.shader != nullptr) {
    if (input::IS_KEY_DOWN(input::KeyCode::LeftShift)) {
      m_debug_depth.fullscreen = !m_debug_depth.fullscreen;
    } else {
      m_debug_depth.active = !m_debug_depth.active;
    }
  }

  if (input::IS_KEY_RELEASED(input::KeyCode::F3) &&
      m_debug_normal.shader != nullptr) {
    m_debug_normal.active = !m_debug_normal.active;
  }

  if ((!m_debug_depth.active || m_shadow_map == nullptr ||
       m_debug_depth.shader == nullptr) &&
      (!m_debug_normal.active || m_debug_normal.shader == nullptr)) {
    return;
  }

  m_scene_color->bind();

  if (m_debug_depth.active && m_shadow_map != nullptr &&
      m_debug_depth.shader != nullptr) {
    draw_depth_overlay();
  }

  if (m_debug_normal.active && m_debug_normal.shader != nullptr) {
    draw_normal_overlay();
  }

  m_scene_color->unbind();
}

void DebugOverlayPass::end(double) {}

void DebugOverlayPass::cleanup() {
  release_quad(m_debug_depth);
  m_debug_normal.shader.reset();
  m_scene_color = nullptr;
  m_shadow_map = nullptr;
}

void DebugOverlayPass::draw_depth_overlay() {
  auto shader = m_debug_depth.shader;
  shader->bind();
  const auto shadow = resolve_debug_shadow_settings();

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
  using namespace shader_bindings::engine_shaders_debug_depth_axsl;
  shader->set(DepthUniform::depth_map, 0);
  shader->set(DepthUniform::fullscreen, m_debug_depth.fullscreen ? 1 : 0);
  shader->set(DepthUniform::near_plane, shadow.near_plane);
  shader->set(DepthUniform::far_plane, shadow.far_plane);
#else
  shader->set_int("depth.depth_map", 0);
  shader->set_int("depth.fullscreen", m_debug_depth.fullscreen ? 1 : 0);
  shader->set_float("depth.near_plane", shadow.near_plane);
  shader->set_float("depth.far_plane", shadow.far_plane);
#endif

  auto renderer_api = m_render_target->renderer_api();
  renderer_api->bind_texture_2d(m_shadow_map->get_depth_attachment_id(), 0);

  renderer_api->disable_depth_test();
  renderer_api->disable_depth_write();
  renderer_api->draw_indexed(m_debug_depth.mesh.vertex_array,
                             m_debug_depth.mesh.draw_type);
  renderer_api->enable_depth_write();
  renderer_api->enable_depth_test();

  shader->unbind();
}

void DebugOverlayPass::draw_normal_overlay() {
  auto scene = SceneManager::get()->get_active_scene();
  if (scene == nullptr) {
    return;
  }

  auto &world = scene->world();
  auto camera = rendering::select_main_camera(world);
  if (!camera.has_value()) {
    return;
  }

  auto shader = m_debug_normal.shader;
  shader->bind();
  shader->set_bool("use_instacing", false);
  shader->set_matrix("view", camera->camera->view_matrix);
  shader->set_matrix("projection", camera->camera->projection_matrix);
  shader->set_float("length", 1.0f);

  world.each<rendering::Renderable, scene::Transform>(
      [&](EntityID entity_id, rendering::Renderable &,
          scene::Transform &transform) {
        if (!world.active(entity_id)) {
          return;
        }

        auto entity = world.entity(entity_id);
        auto *model_ref = entity.get<rendering::ModelRef>();
        auto *mesh_set = entity.get<rendering::MeshSet>();
        if (model_ref == nullptr && mesh_set == nullptr) {
          return;
        }

        shader->set_matrix("g_model", transform.matrix);

        rendering::for_each_render_mesh(
            model_ref, mesh_set, m_render_target, [&](Mesh &mesh) {
              m_render_target->renderer_api()->draw_indexed(
                  mesh.vertex_array, RendererAPI::DrawPrimitive::POINTS);
            });
      });

  shader->unbind();
}

} // namespace astralix
