#include "systems/render-system/passes/editor-gizmo-pass.hpp"

#include "editor-gizmo-store.hpp"
#include "trace.hpp"
#include "editor-selection-store.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "path.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "systems/render-system/scene-selection.hpp"
#include "targets/render-target.hpp"
#include "tools/viewport/gizmo-math.hpp"

#include <algorithm>

namespace astralix {
namespace {

using editor::editor_gizmo_store;
using editor::editor_selection_store;
using editor::EditorGizmoHandle;
namespace gizmo = editor::gizmo;

glm::vec4 base_color(EditorGizmoHandle handle) {
  switch (handle) {
    case EditorGizmoHandle::TranslateX:
    case EditorGizmoHandle::RotateX:
    case EditorGizmoHandle::ScaleX:
      return glm::vec4(0.95f, 0.34f, 0.34f, 1.0f);
    case EditorGizmoHandle::TranslateY:
    case EditorGizmoHandle::RotateY:
    case EditorGizmoHandle::ScaleY:
      return glm::vec4(0.41f, 0.86f, 0.46f, 1.0f);
    case EditorGizmoHandle::TranslateZ:
    case EditorGizmoHandle::RotateZ:
    case EditorGizmoHandle::ScaleZ:
      return glm::vec4(0.37f, 0.58f, 0.96f, 1.0f);
    case EditorGizmoHandle::None:
    default:
      return glm::vec4(0.88f, 0.88f, 0.88f, 1.0f);
  }
}

glm::vec4 resolved_color(
    EditorGizmoHandle handle,
    std::optional<EditorGizmoHandle> hovered_handle,
    std::optional<EditorGizmoHandle> active_handle
) {
  if (active_handle.has_value() && *active_handle == handle) {
    return glm::vec4(1.0f, 0.93f, 0.56f, 1.0f);
  }
  if (hovered_handle.has_value() && *hovered_handle == handle) {
    return glm::vec4(1.0f, 0.83f, 0.42f, 1.0f);
  }

  return base_color(handle);
}

} // namespace

void EditorGizmoPass::setup(
    Ref<RenderTarget> render_target,
    const std::vector<const RenderGraphResource *> &resources
) {
  m_render_target = render_target;
  m_scene_color = nullptr;
  set_enabled(true);

  for (const auto *resource : resources) {
    if (resource->desc.type == RenderGraphResourceType::Framebuffer &&
        resource->desc.name == "scene_color") {
      m_scene_color = resource->get_framebuffer();
    }
  }

  if (m_scene_color == nullptr) {
    set_enabled(false);
    return;
  }

  Shader::create(
      "shaders::editor_gizmo",
      "shaders/editor_gizmo.axsl"_engine,
      "shaders/editor_gizmo.axsl"_engine
  );

  resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
      m_render_target->renderer_api()->get_backend(),
      {"shaders::editor_gizmo"}
  );
  m_shader =
      resource_manager()->get_by_descriptor_id<Shader>("shaders::editor_gizmo");

  if (m_shader == nullptr) {
    set_enabled(false);
  }
}

void EditorGizmoPass::begin(double) {}

void EditorGizmoPass::execute(double) {
  ASTRA_PROFILE_N("EditorGizmoPass");
  if (m_scene_color == nullptr || m_shader == nullptr) {
    return;
  }

  auto *scene = SceneManager::get()->get_active_scene();
  if (scene == nullptr) {
    return;
  }

  auto gizmo_store = editor_gizmo_store();
  const auto selected_entity_id = editor_selection_store()->selected_entity();
  const auto panel_rect = gizmo_store->panel_rect();
  const auto window_rect = gizmo_store->window_rect();
  const bool draw_window_target =
      gizmo_store->window_capture_enabled() && window_rect.has_value();
  const bool draw_panel_target = panel_rect.has_value();
  if ((!draw_panel_target && !draw_window_target) ||
      !selected_entity_id.has_value()) {
    return;
  }

  auto &world = scene->world();
  if (!world.contains(*selected_entity_id)) {
    return;
  }

  auto *transform = world.get<scene::Transform>(*selected_entity_id);
  auto camera_selection = rendering::select_main_camera(world);
  if (transform == nullptr || !camera_selection.has_value() ||
      camera_selection->transform == nullptr || camera_selection->camera == nullptr) {
    return;
  }

  const gizmo::CameraFrame camera_frame = gizmo::make_camera_frame(
      *camera_selection->transform,
      *camera_selection->camera
  );
  auto renderer_api = m_render_target->renderer_api();
  const auto draw_target = [&](const ui::UIRect &target_rect, bool to_default_framebuffer) {
    const float gizmo_scale = gizmo::gizmo_scale_world(
        camera_frame,
        transform->position,
        target_rect.height
    );

    const auto hovered_handle = gizmo_store->hovered_handle();
    const auto active_handle = gizmo_store->active_handle();
    const auto vertices = gizmo::build_gizmo_mesh(
        gizmo_store->mode(),
        transform->position,
        gizmo_scale,
        [&](EditorGizmoHandle handle) {
          return resolved_color(handle, hovered_handle, active_handle);
        }
    );
    if (vertices.empty()) {
      return;
    }

    ensure_mesh_resources(vertices.size());
    if (m_vertex_buffer == nullptr || m_vertex_array == nullptr) {
      return;
    }

    if (to_default_framebuffer) {
      m_render_target->framebuffer()->bind(FramebufferBindType::Default, 0);
    } else {
      m_scene_color->bind();
    }

    renderer_api->disable_depth_test();
    renderer_api->disable_depth_write();
    renderer_api->enable_blend();
    renderer_api->set_blend_func(
        RendererAPI::BlendFactor::SrcAlpha,
        RendererAPI::BlendFactor::OneMinusSrcAlpha
    );

    m_shader->bind();
    m_shader->set_matrix("camera.view", camera_frame.view);
    m_shader->set_matrix("camera.projection", camera_frame.projection);
    m_vertex_buffer->set_data(
        vertices.data(),
        static_cast<uint32_t>(vertices.size() * sizeof(gizmo::GizmoMeshVertex))
    );
    renderer_api->draw_triangles(
        m_vertex_array,
        static_cast<uint32_t>(vertices.size())
    );
    m_shader->unbind();

    renderer_api->disable_blend();
    renderer_api->enable_depth_write();
    renderer_api->enable_depth_test();

    if (to_default_framebuffer) {
      m_render_target->framebuffer()->unbind();
    } else {
      m_scene_color->unbind();
    }
  };

  if (draw_panel_target) {
    draw_target(*panel_rect, false);
  }
  if (draw_window_target) {
    draw_target(*window_rect, true);
  }
}

void EditorGizmoPass::end(double) {}

void EditorGizmoPass::cleanup() {
  m_scene_color = nullptr;
  m_shader.reset();
  m_vertex_array.reset();
  m_vertex_buffer.reset();
  m_vertex_capacity = 0u;
}

void EditorGizmoPass::ensure_mesh_resources(size_t required_vertices) {
  if (required_vertices == 0u) {
    return;
  }

  if (m_vertex_array != nullptr && m_vertex_buffer != nullptr &&
      required_vertices <= m_vertex_capacity) {
    return;
  }

  const auto backend = m_render_target->renderer_api()->get_backend();
  m_vertex_capacity = std::max(required_vertices, size_t(8192u));
  m_vertex_array = VertexArray::create(backend);
  m_vertex_buffer = VertexBuffer::create(
      backend,
      static_cast<uint32_t>(m_vertex_capacity * sizeof(gizmo::GizmoMeshVertex))
  );

  BufferLayout layout(
      {BufferElement(ShaderDataType::Float3, "a_position"),
       BufferElement(ShaderDataType::Float3, "a_normal"),
       BufferElement(ShaderDataType::Float4, "a_color")}
  );
  m_vertex_buffer->set_layout(layout);
  m_vertex_array->add_vertex_buffer(m_vertex_buffer);
  m_vertex_array->unbind();
}

} // namespace astralix
