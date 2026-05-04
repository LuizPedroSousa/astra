#include "systems/render-system/passes/navigation-gizmo-pass.hpp"

#include "editor-viewport-navigation-store.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "managers/scene-manager.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "targets/render-target.hpp"
#include "tools/navigation-gizmo/navigation-gizmo-shared.hpp"
#include "tools/viewport/gizmo-math.hpp"
#include "vertex-buffer.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

#include <algorithm>
#include <vector>

namespace astralix {
namespace {

namespace gizmo = editor::gizmo;
namespace nav = editor::navigation_gizmo;
using editor::editor_viewport_navigation_store;

struct MeshBatch {
  std::vector<gizmo::GizmoMeshVertex> vertices;
  float depth = 0.0f;
};

glm::vec4 blend_color(
    const glm::vec4 &lhs,
    const glm::vec4 &rhs,
    float factor
) {
  return lhs * (1.0f - factor) + rhs * factor;
}

glm::vec4 resolved_color(const nav::NavigationMarkerInstance &marker) {
  if (!marker.active) {
    return marker.config.color;
  }

  return blend_color(marker.config.color, glm::vec4(1.0f), 0.26f);
}

glm::mat4 rotation_matrix(const glm::mat3 &rotation) {
  glm::mat4 matrix(1.0f);
  matrix[0] = glm::vec4(rotation[0], 0.0f);
  matrix[1] = glm::vec4(rotation[1], 0.0f);
  matrix[2] = glm::vec4(rotation[2], 0.0f);
  return matrix;
}

std::optional<ui::UIRect> clamp_draw_rect(
    const std::optional<ui::UIRect> &rect,
    const ImageExtent &extent
) {
  if (!rect.has_value()) {
    return std::nullopt;
  }

  const float left = std::clamp(rect->x, 0.0f, static_cast<float>(extent.width));
  const float top = std::clamp(rect->y, 0.0f, static_cast<float>(extent.height));
  const float right =
      std::clamp(rect->right(), 0.0f, static_cast<float>(extent.width));
  const float bottom =
      std::clamp(rect->bottom(), 0.0f, static_cast<float>(extent.height));
  if (right - left <= 1.0f || bottom - top <= 1.0f) {
    return std::nullopt;
  }

  return ui::UIRect{
      .x = left,
      .y = top,
      .width = right - left,
      .height = bottom - top,
  };
}

glm::mat4 subrect_clip_matrix(
    const ui::UIRect &rect,
    const ImageExtent &extent
) {
  const float framebuffer_width = std::max(1.0f, static_cast<float>(extent.width));
  const float framebuffer_height =
      std::max(1.0f, static_cast<float>(extent.height));
  const float left = (rect.x / framebuffer_width) * 2.0f - 1.0f;
  const float right = (rect.right() / framebuffer_width) * 2.0f - 1.0f;
  const float top = 1.0f - (rect.y / framebuffer_height) * 2.0f;
  const float bottom = 1.0f - (rect.bottom() / framebuffer_height) * 2.0f;

  glm::mat4 matrix(1.0f);
  matrix[0][0] = (right - left) * 0.5f;
  matrix[1][1] = (top - bottom) * 0.5f;
  matrix[3][0] = (right + left) * 0.5f;
  matrix[3][1] = (top + bottom) * 0.5f;
  return matrix;
}

glm::mat4 navigation_view_matrix(const glm::mat3 &view_rotation) {
  return glm::translate(
             glm::mat4(1.0f),
             glm::vec3(0.0f, 0.0f, -nav::k_camera_distance)
         ) *
         rotation_matrix(view_rotation);
}

glm::mat4 navigation_projection_matrix(
    const ui::UIRect &draw_rect,
    const ImageExtent &extent
) {
  const float aspect = std::max(draw_rect.width, 1.0f) /
                       std::max(draw_rect.height, 1.0f);
  const glm::mat4 base_projection = glm::ortho(
      -nav::k_orthographic_extent * aspect,
      nav::k_orthographic_extent * aspect,
      -nav::k_orthographic_extent,
      nav::k_orthographic_extent,
      0.1f,
      10.0f
  );
  return subrect_clip_matrix(draw_rect, extent) * base_projection;
}

MeshBatch build_axis_batch(const nav::NavigationMarkerInstance &marker) {
  MeshBatch batch;
  batch.vertices.reserve(256u);

  const glm::vec3 axis = marker.config.axis;
  const glm::vec4 color = resolved_color(marker);

  if (marker.config.prominent) {
    gizmo::append_cylinder(
        batch.vertices,
        glm::vec3(0.0f),
        axis * nav::k_positive_shaft_length,
        nav::k_positive_shaft_radius,
        color
    );
    gizmo::append_cone(
        batch.vertices,
        axis * nav::k_positive_shaft_length,
        axis * marker.config.tip_length,
        nav::k_positive_head_radius,
        color
    );
  } else {
    gizmo::append_cylinder(
        batch.vertices,
        glm::vec3(0.0f),
        axis * nav::k_negative_shaft_length,
        nav::k_negative_shaft_radius,
        color
    );
    gizmo::append_cube(
        batch.vertices,
        axis * marker.config.tip_length,
        nav::k_negative_cap_half,
        color
    );
  }

  batch.depth = marker.view_axis.z * marker.config.tip_length;
  return batch;
}

std::vector<gizmo::GizmoMeshVertex> build_navigation_mesh(
    const rendering::CameraSelection &selection
) {
  std::vector<MeshBatch> batches;
  batches.reserve(7u);

  for (const auto &marker : nav::navigation_marker_instances(selection)) {
    batches.push_back(build_axis_batch(marker));
  }

  MeshBatch center_batch;
  center_batch.vertices.reserve(36u);
  gizmo::append_cube(
      center_batch.vertices,
      glm::vec3(0.0f),
      nav::k_origin_cube_half,
      glm::vec4(0.16f, 0.17f, 0.20f, 1.0f)
  );
  center_batch.depth = 0.0f;
  batches.push_back(std::move(center_batch));

  std::sort(
      batches.begin(),
      batches.end(),
      [](const MeshBatch &lhs, const MeshBatch &rhs) {
        return lhs.depth < rhs.depth;
      }
  );

  size_t vertex_count = 0u;
  for (const auto &batch : batches) {
    vertex_count += batch.vertices.size();
  }

  std::vector<gizmo::GizmoMeshVertex> vertices;
  vertices.reserve(vertex_count);
  for (auto &batch : batches) {
    vertices.insert(
        vertices.end(),
        batch.vertices.begin(),
        batch.vertices.end()
    );
  }

  return vertices;
}

} // namespace

void NavigationGizmoPass::setup(PassSetupContext &ctx) {
  m_shader = ctx.require_shader("navigation_gizmo_shader");

  if (m_shader == nullptr) {
    set_enabled(false);
  }
}

void NavigationGizmoPass::record(
    PassRecordContext &ctx, PassRecorder &recorder
) {
  const auto *scene_color_resource = ctx.find_graph_image("scene_color");
  if (scene_color_resource == nullptr || m_shader == nullptr) {
    return;
  }

  auto *scene = SceneManager::get()->get_active_scene();
  if (scene == nullptr) {
    return;
  }

  auto camera_selection = nav::active_viewport_camera_selection();
  if (!camera_selection.has_value() || camera_selection->camera == nullptr) {
    return;
  }

  const auto extent = ctx.graph_image_extent(*scene_color_resource);
  const auto draw_rect = clamp_draw_rect(
      editor_viewport_navigation_store()->draw_rect(),
      extent
  );
  if (!draw_rect.has_value()) {
    return;
  }

  const auto vertices = build_navigation_mesh(*camera_selection);
  if (vertices.empty()) {
    return;
  }

  auto &frame = ctx.frame();
  auto scene_color = ctx.register_graph_image(
      "navigation-gizmo.scene-color", *scene_color_resource
  );

  BufferLayout layout(
      {BufferElement(ShaderDataType::Float3, "a_position").at_location(0),
       BufferElement(ShaderDataType::Float3, "a_normal").at_location(1),
       BufferElement(ShaderDataType::Float4, "a_color").at_location(2)}
  );

  const auto vertex_buffer = frame.register_transient_vertices(
      "navigation-gizmo.vertices",
      vertices.data(),
      static_cast<uint32_t>(vertices.size() * sizeof(gizmo::GizmoMeshVertex)),
      static_cast<uint32_t>(vertices.size()),
      layout
  );

  const auto bindings = frame.register_binding_group(
      make_binding_group_desc(
          "navigation-gizmo-pass",
          "navigation-gizmo-pass",
          m_shader,
          0,
          "navigation-gizmo-pass",
          RenderBindingScope::Pass,
          RenderBindingCachePolicy::Reuse,
          RenderBindingSharing::LocalOnly,
          0,
          RenderBindingStability::FrameLocal
      )
  );

  const glm::mat3 view_rotation(camera_selection->camera->view_matrix);
  rendering::record_shader_params(
      frame,
      bindings,
      shader_bindings::engine_shaders_editor_gizmo_axsl::CameraParams{
          .view = navigation_view_matrix(view_rotation),
          .projection = navigation_projection_matrix(*draw_rect, extent),
      }
  );

  RenderPipelineDesc pipeline_desc;
  pipeline_desc.debug_name = "navigation-gizmo-pass";
  pipeline_desc.depth_stencil.depth_test = false;
  pipeline_desc.depth_stencil.depth_write = false;
  pipeline_desc.blend_attachments = {
      BlendAttachmentState::replace(),
  };

  const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);

  RenderingInfo info;
  info.debug_name = "navigation-gizmo-pass";
  info.extent = extent;
  info.color_attachments.push_back(ColorAttachmentRef{
      .view = ImageViewRef{.image = scene_color},
      .load_op = AttachmentLoadOp::Load,
      .store_op = AttachmentStoreOp::Store,
  });

  const uint32_t scissor_x = static_cast<uint32_t>(draw_rect->x);
  const uint32_t scissor_y = static_cast<uint32_t>(draw_rect->y);
  const uint32_t scissor_width = static_cast<uint32_t>(draw_rect->width);
  const uint32_t scissor_height = static_cast<uint32_t>(draw_rect->height);

  recorder.begin_rendering(info);
  recorder.set_scissor(
      true,
      scissor_x,
      scissor_y,
      scissor_width,
      scissor_height
  );
  recorder.bind_pipeline(pipeline);
  recorder.bind_binding_group(bindings);
  recorder.bind_vertex_buffer(vertex_buffer);
  recorder.draw_vertices(static_cast<uint32_t>(vertices.size()));
  recorder.set_scissor(false);
  recorder.end_rendering();
}

} // namespace astralix
