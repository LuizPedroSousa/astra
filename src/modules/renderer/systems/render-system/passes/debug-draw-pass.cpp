#include "systems/render-system/passes/debug-draw-pass.hpp"

#include "managers/debug-draw-store.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/render-frame.hpp"
#include "vertex-buffer.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "glm/gtc/constants.hpp"

namespace astralix {
namespace {

constexpr float k_epsilon = 0.0001f;
constexpr int k_mesh_circle_segments = 10;

struct DebugDrawVertex {
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec4 color = glm::vec4(1.0f);
};

struct DebugDrawBatches {
  std::vector<DebugDrawVertex> depth;
  std::vector<DebugDrawVertex> no_depth;
  std::vector<DebugDrawVertex> xray_back;
  std::vector<DebugDrawVertex> xray_front;

  bool empty() const {
    return depth.empty() && no_depth.empty() &&
           xray_back.empty() && xray_front.empty();
  }
};

glm::vec3 normalized_or_fallback(glm::vec3 value, glm::vec3 fallback) {
  return glm::dot(value, value) > k_epsilon ? glm::normalize(value) : fallback;
}

glm::vec3 fallback_perpendicular(glm::vec3 axis) {
  const glm::vec3 reference =
      std::abs(axis.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f)
                              : glm::vec3(0.0f, 1.0f, 0.0f);
  return normalized_or_fallback(glm::cross(axis, reference), glm::vec3(0.0f, 0.0f, 1.0f));
}

std::pair<glm::vec3, glm::vec3> orthonormal_basis(glm::vec3 axis) {
  axis = normalized_or_fallback(axis, glm::vec3(0.0f, 1.0f, 0.0f));
  const glm::vec3 tangent = fallback_perpendicular(axis);
  const glm::vec3 bitangent =
      normalized_or_fallback(glm::cross(axis, tangent), glm::vec3(0.0f, 0.0f, 1.0f));
  return {tangent, bitangent};
}

glm::vec3 transform_point(
    const std::optional<glm::mat4> &transform,
    const glm::vec3 &point
) {
  if (!transform.has_value()) {
    return point;
  }

  return glm::vec3(*transform * glm::vec4(point, 1.0f));
}

float world_units_per_pixel(
    const rendering::CameraFrame &camera,
    const glm::vec3 &pivot,
    float viewport_height
) {
  const float safe_height = std::max(viewport_height, 1.0f);
  if (camera.orthographic) {
    return (camera.orthographic_scale * 2.0f) / safe_height;
  }

  const float distance =
      std::max(glm::length(pivot - camera.position), k_epsilon);
  const float vertical_extent =
      2.0f * distance *
      std::tan(glm::radians(camera.fov_degrees) * 0.5f);
  return vertical_extent / safe_height;
}

void append_cylinder(
    std::vector<DebugDrawVertex> &vertices,
    const glm::vec3 &base,
    const glm::vec3 &tip,
    float radius,
    const glm::vec4 &color,
    int segments = k_mesh_circle_segments
) {
  const glm::vec3 axis = tip - base;
  if (glm::dot(axis, axis) < k_epsilon || radius <= 0.0f) {
    return;
  }
  const auto [tangent, bitangent] = orthonormal_basis(axis);

  for (int i = 0; i < segments; ++i) {
    const float angle_current =
        glm::two_pi<float>() * static_cast<float>(i) /
        static_cast<float>(segments);
    const float angle_next =
        glm::two_pi<float>() * static_cast<float>(i + 1) /
        static_cast<float>(segments);

    const glm::vec3 normal_current =
        tangent * std::cos(angle_current) +
        bitangent * std::sin(angle_current);
    const glm::vec3 normal_next =
        tangent * std::cos(angle_next) +
        bitangent * std::sin(angle_next);

    const glm::vec3 base_current = base + normal_current * radius;
    const glm::vec3 base_next = base + normal_next * radius;
    const glm::vec3 tip_current = tip + normal_current * radius;
    const glm::vec3 tip_next = tip + normal_next * radius;

    vertices.push_back({base_current, normal_current, color});
    vertices.push_back({base_next, normal_next, color});
    vertices.push_back({tip_current, normal_current, color});

    vertices.push_back({tip_current, normal_current, color});
    vertices.push_back({base_next, normal_next, color});
    vertices.push_back({tip_next, normal_next, color});
  }
}

void append_cone(
    std::vector<DebugDrawVertex> &vertices,
    const glm::vec3 &base,
    const glm::vec3 &tip,
    float radius,
    const glm::vec4 &color,
    int segments = k_mesh_circle_segments
) {
  const glm::vec3 axis = tip - base;
  const float length = glm::length(axis);
  if (length < k_epsilon || radius <= 0.0f) {
    return;
  }
  const glm::vec3 direction = axis / length;
  const float slope = radius / length;
  const auto [tangent, bitangent] = orthonormal_basis(axis);

  for (int i = 0; i < segments; ++i) {
    const float angle_current =
        glm::two_pi<float>() * static_cast<float>(i) /
        static_cast<float>(segments);
    const float angle_next =
        glm::two_pi<float>() * static_cast<float>(i + 1) /
        static_cast<float>(segments);

    const glm::vec3 radial_current =
        tangent * std::cos(angle_current) +
        bitangent * std::sin(angle_current);
    const glm::vec3 radial_next =
        tangent * std::cos(angle_next) +
        bitangent * std::sin(angle_next);

    const glm::vec3 base_current = base + radial_current * radius;
    const glm::vec3 base_next = base + radial_next * radius;

    const glm::vec3 side_normal_current =
        glm::normalize(radial_current + direction * slope);
    const glm::vec3 side_normal_next =
        glm::normalize(radial_next + direction * slope);
    const glm::vec3 tip_normal =
        glm::normalize(side_normal_current + side_normal_next);

    vertices.push_back({base_current, side_normal_current, color});
    vertices.push_back({base_next, side_normal_next, color});
    vertices.push_back({tip, tip_normal, color});

    vertices.push_back({base, -direction, color});
    vertices.push_back({base_next, -direction, color});
    vertices.push_back({base_current, -direction, color});
  }
}

void append_cube(
    std::vector<DebugDrawVertex> &vertices,
    const glm::vec3 &center,
    float half_size,
    const glm::vec4 &color
) {
  if (half_size <= 0.0f) {
    return;
  }

  struct CubeFace {
    glm::vec3 normal;
    glm::vec3 up;
    glm::vec3 right;
  };

  const CubeFace faces[] = {
      {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}},
      {{0, 0, -1}, {0, 1, 0}, {-1, 0, 0}},
      {{1, 0, 0}, {0, 1, 0}, {0, 0, -1}},
      {{-1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
      {{0, 1, 0}, {0, 0, -1}, {1, 0, 0}},
      {{0, -1, 0}, {0, 0, 1}, {1, 0, 0}},
  };

  for (const auto &face : faces) {
    const glm::vec3 face_center = center + face.normal * half_size;
    const glm::vec3 right = face.right * half_size;
    const glm::vec3 up = face.up * half_size;

    const glm::vec3 v0 = face_center - right - up;
    const glm::vec3 v1 = face_center + right - up;
    const glm::vec3 v2 = face_center + right + up;
    const glm::vec3 v3 = face_center - right + up;

    vertices.push_back({v0, face.normal, color});
    vertices.push_back({v1, face.normal, color});
    vertices.push_back({v2, face.normal, color});

    vertices.push_back({v0, face.normal, color});
    vertices.push_back({v2, face.normal, color});
    vertices.push_back({v3, face.normal, color});
  }
}

float resolved_line_radius(
    const DebugDrawStyle &style,
    const rendering::CameraFrame &camera,
    const glm::vec3 &start,
    const glm::vec3 &end,
    float viewport_height
) {
  if (style.thickness <= 0.0f) {
    return 0.0f;
  }

  if (style.size_space == DebugDrawSizeSpace::World) {
    return style.thickness * 0.5f;
  }

  const glm::vec3 midpoint = (start + end) * 0.5f;
  return world_units_per_pixel(camera, midpoint, viewport_height) *
         style.thickness * 0.5f;
}

float resolved_point_half_extent(
    const DebugDrawPointStyle &style,
    const rendering::CameraFrame &camera,
    const glm::vec3 &position,
    float viewport_height
) {
  if (style.size <= 0.0f) {
    return 0.0f;
  }

  if (style.size_space == DebugDrawSizeSpace::World) {
    return style.size * 0.5f;
  }

  return world_units_per_pixel(camera, position, viewport_height) *
         style.size * 0.5f;
}

glm::vec4 xray_back_color(const glm::vec4 &color) {
  return glm::vec4(color.r, color.g, color.b, color.a * 0.24f);
}

void append_line_mesh(
    std::vector<DebugDrawVertex> &vertices,
    const glm::vec3 &start,
    const glm::vec3 &end,
    float radius,
    const glm::vec4 &color
) {
  append_cylinder(vertices, start, end, radius, color);
}

void append_arrow_mesh(
    std::vector<DebugDrawVertex> &vertices,
    const glm::vec3 &start,
    const glm::vec3 &end,
    float radius,
    const glm::vec4 &color
) {
  const glm::vec3 axis = end - start;
  const float length = glm::length(axis);
  if (length <= k_epsilon) {
    return;
  }

  const glm::vec3 direction = axis / length;
  const float head_length =
      std::min(length * 0.45f, std::max(length * 0.22f, radius * 3.0f));
  const glm::vec3 shaft_end = end - direction * head_length;

  append_cylinder(vertices, start, shaft_end, radius, color);
  append_cone(vertices, shaft_end, end, radius * 2.25f, color);
}

void append_line_command(
    DebugDrawBatches &batches,
    const DebugDrawLineCommand &command,
    const rendering::CameraFrame &camera,
    float viewport_height
) {
  const glm::vec3 start =
      transform_point(command.style.transform, command.start);
  const glm::vec3 end =
      transform_point(command.style.transform, command.end);
  const float radius =
      resolved_line_radius(command.style, camera, start, end, viewport_height);
  if (radius <= 0.0f) {
    return;
  }

  auto append = [&](std::vector<DebugDrawVertex> &target,
                    const glm::vec4 &color) {
    if (command.arrow) {
      append_arrow_mesh(target, start, end, radius, color);
    } else {
      append_line_mesh(target, start, end, radius, color);
    }
  };

  switch (command.style.depth_mode) {
    case DebugDrawDepthMode::NoDepth:
      append(batches.no_depth, command.style.color);
      break;
    case DebugDrawDepthMode::XRay:
      append(batches.xray_back, xray_back_color(command.style.color));
      append(batches.xray_front, command.style.color);
      break;
    case DebugDrawDepthMode::DepthTest:
    default:
      append(batches.depth, command.style.color);
      break;
  }
}

void append_point_command(
    DebugDrawBatches &batches,
    const DebugDrawPointCommand &command,
    const rendering::CameraFrame &camera,
    float viewport_height
) {
  const glm::vec3 position =
      transform_point(command.style.transform, command.position);
  const float half_extent = resolved_point_half_extent(
      command.style, camera, position, viewport_height
  );
  if (half_extent <= 0.0f) {
    return;
  }

  auto append = [&](std::vector<DebugDrawVertex> &target,
                    const glm::vec4 &color) {
    append_cube(target, position, half_extent, color);
  };

  switch (command.style.depth_mode) {
    case DebugDrawDepthMode::NoDepth:
      append(batches.no_depth, command.style.color);
      break;
    case DebugDrawDepthMode::XRay:
      append(batches.xray_back, xray_back_color(command.style.color));
      append(batches.xray_front, command.style.color);
      break;
    case DebugDrawDepthMode::DepthTest:
    default:
      append(batches.depth, command.style.color);
      break;
  }
}

DebugDrawBatches build_batches(
    const DebugDrawStore &store,
    const rendering::CameraFrame &camera,
    float viewport_height
) {
  DebugDrawBatches batches;

  for (const auto &command : store.lines()) {
    if (!store.category_enabled(command.style.category)) {
      continue;
    }
    append_line_command(batches, command, camera, viewport_height);
  }

  for (const auto &command : store.points()) {
    if (!store.category_enabled(command.style.category)) {
      continue;
    }
    append_point_command(batches, command, camera, viewport_height);
  }

  return batches;
}

BufferLayout debug_draw_layout() {
  return BufferLayout({
      BufferElement(ShaderDataType::Float3, "a_position").at_location(0),
      BufferElement(ShaderDataType::Float3, "a_normal").at_location(1),
      BufferElement(ShaderDataType::Float4, "a_color").at_location(2),
  });
}

} // namespace

void DebugDrawPass::setup(PassSetupContext &ctx) {
  m_shader = ctx.require_shader("debug_draw_shader");
  if (m_shader == nullptr) {
    set_enabled(false);
  }
}

void DebugDrawPass::record(
    PassRecordContext &ctx, PassRecorder &recorder
) {
  const auto *scene_color_resource = ctx.find_graph_image("scene_color");
  const auto *scene_depth_resource = ctx.find_graph_image("scene_depth");
  const auto *scene_frame = ctx.scene();
  if (scene_color_resource == nullptr || m_shader == nullptr ||
      scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
    return;
  }

  const auto store = debug_draw();
  if (store == nullptr || store->empty()) {
    return;
  }

  auto batches = build_batches(
      *store,
      *scene_frame->main_camera,
      static_cast<float>(ctx.graph_image_extent(*scene_color_resource).height)
  );
  if (batches.empty()) {
    return;
  }

  auto &frame = ctx.frame();
  const auto extent = ctx.graph_image_extent(*scene_color_resource);
  auto scene_color = ctx.register_graph_image(
      "debug-draw.scene-color", *scene_color_resource
  );
  const auto scene_depth =
      scene_depth_resource != nullptr
          ? ctx.register_graph_image(
                "debug-draw.scene-depth",
                *scene_depth_resource,
                ImageAspect::Depth
            )
          : ImageHandle{};

  if (!scene_depth.valid()) {
    batches.no_depth.insert(
        batches.no_depth.end(),
        batches.depth.begin(),
        batches.depth.end()
    );
    batches.no_depth.insert(
        batches.no_depth.end(),
        batches.xray_front.begin(),
        batches.xray_front.end()
    );
    batches.depth.clear();
    batches.xray_front.clear();
  }

  const auto bindings = frame.register_binding_group(
      make_binding_group_desc(
          "debug-draw-pass",
          "debug-draw-pass",
          m_shader,
          0,
          "debug-draw-pass",
          RenderBindingScope::Pass,
          RenderBindingCachePolicy::Reuse,
          RenderBindingSharing::LocalOnly,
          0,
          RenderBindingStability::FrameLocal
      )
  );
  rendering::record_shader_params(
      frame,
      bindings,
      shader_bindings::engine_shaders_editor_gizmo_axsl::CameraParams{
          .view = scene_frame->main_camera->view,
          .projection = scene_frame->main_camera->projection,
      }
  );

  RenderPipelineDesc no_depth_pipeline_desc;
  no_depth_pipeline_desc.debug_name = "debug-draw.no-depth";
  no_depth_pipeline_desc.depth_stencil.depth_test = false;
  no_depth_pipeline_desc.depth_stencil.depth_write = false;
  no_depth_pipeline_desc.blend_attachments = {
      BlendAttachmentState::alpha_blend(),
  };

  RenderPipelineDesc depth_pipeline_desc = no_depth_pipeline_desc;
  depth_pipeline_desc.debug_name = "debug-draw.depth";
  depth_pipeline_desc.depth_stencil.depth_test = true;
  depth_pipeline_desc.depth_stencil.compare_op = CompareOp::LessEqual;

  const auto no_depth_pipeline =
      frame.register_pipeline(no_depth_pipeline_desc, m_shader);
  const auto depth_pipeline = !batches.depth.empty() || !batches.xray_front.empty()
                                  ? frame.register_pipeline(
                                        depth_pipeline_desc,
                                        m_shader
                                    )
                                  : RenderPipelineHandle{};

  const BufferLayout layout = debug_draw_layout();
  const auto register_vertices = [&](const char *debug_name,
                                     const std::vector<DebugDrawVertex> &vertices) {
    return frame.register_transient_vertices(
        debug_name,
        vertices.data(),
        static_cast<uint32_t>(vertices.size() * sizeof(DebugDrawVertex)),
        static_cast<uint32_t>(vertices.size()),
        layout
    );
  };

  RenderingInfo info;
  info.debug_name = "debug-draw-pass";
  info.extent = extent;
  info.color_attachments.push_back(ColorAttachmentRef{
      .view = ImageViewRef{.image = scene_color},
      .load_op = AttachmentLoadOp::Load,
      .store_op = AttachmentStoreOp::Store,
  });
  if (scene_depth.valid()) {
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
  }

  recorder.begin_rendering(info);
  recorder.bind_binding_group(bindings);

  const auto draw_batch = [&](const char *debug_name,
                              RenderPipelineHandle pipeline,
                              const std::vector<DebugDrawVertex> &vertices) {
    if (vertices.empty() || !pipeline.valid()) {
      return;
    }

    const auto vertex_buffer = register_vertices(debug_name, vertices);
    recorder.bind_pipeline(pipeline);
    recorder.bind_vertex_buffer(vertex_buffer);
    recorder.draw_vertices(static_cast<uint32_t>(vertices.size()));
  };

  draw_batch("debug-draw.no-depth", no_depth_pipeline, batches.no_depth);
  draw_batch("debug-draw.xray-back", no_depth_pipeline, batches.xray_back);
  draw_batch("debug-draw.depth", depth_pipeline, batches.depth);
  draw_batch("debug-draw.xray-front", depth_pipeline, batches.xray_front);

  recorder.end_rendering();
}

} // namespace astralix
