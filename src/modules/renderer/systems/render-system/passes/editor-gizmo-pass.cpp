#include "systems/render-system/passes/editor-gizmo-pass.hpp"

#include "editor-gizmo-store.hpp"
#include "editor-selection-store.hpp"
#include "managers/scene-manager.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/scene-selection.hpp"
#include "targets/render-target.hpp"
#include "tools/viewport/gizmo-math.hpp"
#include "vertex-buffer.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

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

void EditorGizmoPass::setup(PassSetupContext &ctx) {
  m_shader = ctx.require_shader("editor_gizmo_shader");

  if (m_shader == nullptr) {
    LOG_WARN("[EditorGizmoPass::setup] missing graph dependency: editor_gizmo_shader");
    set_enabled(false);
  }
}

void EditorGizmoPass::record(
    PassRecordContext &ctx, PassRecorder &recorder
) {
  ASTRA_PROFILE_N("EditorGizmoPass::record");

  const auto *scene_color_resource = ctx.find_graph_image("scene_color");
  if (scene_color_resource == nullptr || m_shader == nullptr) {
    LOG_WARN("[EditorGizmoPass::record] early exit: null image or shader");
    return;
  }

  auto *scene = SceneManager::get()->get_active_scene();
  if (scene == nullptr) {
    LOG_WARN("[EditorGizmoPass::record] early exit: no active scene");
    return;
  }

  auto gizmo_store = editor_gizmo_store();
  const auto selected_entity_id = editor_selection_store()->selected_entity();
  const auto interaction_rect = gizmo_store->interaction_rect();
  const bool draw_target_available = interaction_rect.has_value();
  if (!draw_target_available || !selected_entity_id.has_value()) {
    static int gizmo_skip_count = 0;
    if (gizmo_skip_count++ < 5) {
      LOG_WARN("[EditorGizmoPass::record] early exit: interaction_rect=",
               draw_target_available, " selected_entity=",
               selected_entity_id.has_value());
    }
    return;
  }

  auto &world = scene->world();
  if (!world.contains(*selected_entity_id)) {
    LOG_WARN("[EditorGizmoPass::record] early exit: selected entity not in world");
    return;
  }

  auto *transform = world.get<scene::Transform>(*selected_entity_id);
  auto camera_selection = rendering::select_main_camera(world);
  if (transform == nullptr || !camera_selection.has_value() ||
      camera_selection->transform == nullptr ||
      camera_selection->camera == nullptr) {
    LOG_WARN("[EditorGizmoPass::record] early exit: missing transform or camera");
    return;
  }

  const gizmo::CameraFrame camera_frame = gizmo::make_camera_frame(
      *camera_selection->transform,
      *camera_selection->camera
  );

  const float gizmo_scale = gizmo::gizmo_scale_world(
      camera_frame,
      transform->position,
      interaction_rect->height
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
    LOG_WARN("[EditorGizmoPass::record] early exit: no gizmo vertices");
    return;
  }

  LOG_INFO("[EditorGizmoPass::record] drawing ", vertices.size(), " vertices, scale=", gizmo_scale);

  auto &frame = ctx.frame();
  const auto extent = ctx.graph_image_extent(*scene_color_resource);

  auto scene_color = ctx.register_graph_image(
      "editor-gizmo.scene-color", *scene_color_resource
  );

  BufferLayout layout(
      {BufferElement(ShaderDataType::Float3, "a_position").at_location(0),
       BufferElement(ShaderDataType::Float3, "a_normal").at_location(1),
       BufferElement(ShaderDataType::Float4, "a_color").at_location(2)}
  );

  const auto vertex_buffer = frame.register_transient_vertices(
      "editor-gizmo.vertices",
      vertices.data(),
      static_cast<uint32_t>(vertices.size() * sizeof(gizmo::GizmoMeshVertex)),
      static_cast<uint32_t>(vertices.size()),
      layout
  );

  const auto bindings = frame.register_binding_group(
      make_binding_group_desc(
          "editor-gizmo-pass",
          "editor-gizmo-pass",
          m_shader,
          0,
          "editor-gizmo-pass",
          RenderBindingScope::Pass,
          RenderBindingCachePolicy::Reuse,
          RenderBindingSharing::LocalOnly,
          0,
          RenderBindingStability::FrameLocal
      )
  );
  rendering::record_shader_params(
      frame, bindings,
      shader_bindings::engine_shaders_editor_gizmo_axsl::CameraParams{
          .view = camera_frame.view,
          .projection = camera_frame.projection,
      }
  );

  RenderPipelineDesc pipeline_desc;
  pipeline_desc.debug_name = "editor-gizmo-pass";
  pipeline_desc.depth_stencil.depth_test = false;
  pipeline_desc.depth_stencil.depth_write = false;
  pipeline_desc.blend_attachments = {
      BlendAttachmentState::alpha_blend(),
  };

  const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);

  RenderingInfo info;
  info.debug_name = "editor-gizmo-pass";
  info.extent = extent;
  info.color_attachments.push_back(ColorAttachmentRef{
      .view = ImageViewRef{.image = scene_color},
      .load_op = AttachmentLoadOp::Load,
      .store_op = AttachmentStoreOp::Store,
  });

  recorder.begin_rendering(info);
  recorder.bind_pipeline(pipeline);
  recorder.bind_binding_group(bindings);
  recorder.bind_vertex_buffer(vertex_buffer);
  recorder.draw_vertices(static_cast<uint32_t>(vertices.size()));
  recorder.end_rendering();
}

} // namespace astralix
