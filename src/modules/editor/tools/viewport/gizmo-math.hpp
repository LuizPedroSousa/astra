#pragma once

#include "components/camera.hpp"
#include "components/transform.hpp"
#include "editor-gizmo-store.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/constants.hpp"
#include "glm/gtc/matrix_inverse.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/norm.hpp"
#include "types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace astralix::editor::gizmo {

inline constexpr float k_target_pixel_size = 96.0f;
inline constexpr float k_handle_pick_threshold_pixels = 12.0f;
inline constexpr float k_ring_radius_ratio = 0.78f;
inline constexpr float k_arrow_head_ratio = 0.18f;
inline constexpr float k_scale_cap_ratio = 0.11f;
inline constexpr int k_ring_segment_count = 48;
inline constexpr float k_min_scale_component = 0.001f;
inline constexpr float k_epsilon = 0.0001f;
inline constexpr int k_mesh_circle_segments = 10;
inline constexpr int k_torus_tube_segments = 8;
inline constexpr float k_shaft_radius_ratio = 0.035f;
inline constexpr float k_cone_radius_ratio = 0.09f;
inline constexpr float k_torus_tube_radius_ratio = 0.028f;

struct CameraFrame {
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::mat4 view = glm::mat4(1.0f);
  glm::mat4 projection = glm::mat4(1.0f);
  bool orthographic = false;
  float fov_degrees = 45.0f;
  float orthographic_scale = 10.0f;
};

struct ScreenRay {
  glm::vec3 origin = glm::vec3(0.0f);
  glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
};

struct GizmoLineSegment {
  glm::vec3 start = glm::vec3(0.0f);
  glm::vec3 end = glm::vec3(0.0f);
  EditorGizmoHandle handle = EditorGizmoHandle::None;
};

struct ProjectedPoint {
  glm::vec2 position = glm::vec2(0.0f);
  bool valid = false;
};

inline EditorGizmoMode mode_from_index(size_t index) {
  switch (index) {
    case 1u:
      return EditorGizmoMode::Rotate;
    case 2u:
      return EditorGizmoMode::Scale;
    case 0u:
    default:
      return EditorGizmoMode::Translate;
  }
}

inline size_t mode_to_index(EditorGizmoMode mode) {
  switch (mode) {
    case EditorGizmoMode::Rotate:
      return 1u;
    case EditorGizmoMode::Scale:
      return 2u;
    case EditorGizmoMode::Translate:
    default:
      return 0u;
  }
}

inline glm::vec3 axis_vector(EditorGizmoHandle handle) {
  switch (handle) {
    case EditorGizmoHandle::TranslateX:
    case EditorGizmoHandle::RotateX:
    case EditorGizmoHandle::ScaleX:
      return glm::vec3(1.0f, 0.0f, 0.0f);
    case EditorGizmoHandle::TranslateY:
    case EditorGizmoHandle::RotateY:
    case EditorGizmoHandle::ScaleY:
      return glm::vec3(0.0f, 1.0f, 0.0f);
    case EditorGizmoHandle::TranslateZ:
    case EditorGizmoHandle::RotateZ:
    case EditorGizmoHandle::ScaleZ:
      return glm::vec3(0.0f, 0.0f, 1.0f);
    case EditorGizmoHandle::None:
    default:
      return glm::vec3(0.0f);
  }
}

inline EditorGizmoMode mode_for_handle(EditorGizmoHandle handle) {
  switch (handle) {
    case EditorGizmoHandle::RotateX:
    case EditorGizmoHandle::RotateY:
    case EditorGizmoHandle::RotateZ:
      return EditorGizmoMode::Rotate;
    case EditorGizmoHandle::ScaleX:
    case EditorGizmoHandle::ScaleY:
    case EditorGizmoHandle::ScaleZ:
      return EditorGizmoMode::Scale;
    case EditorGizmoHandle::TranslateX:
    case EditorGizmoHandle::TranslateY:
    case EditorGizmoHandle::TranslateZ:
    case EditorGizmoHandle::None:
    default:
      return EditorGizmoMode::Translate;
  }
}

inline std::array<EditorGizmoHandle, 3u> handles_for_mode(EditorGizmoMode mode) {
  switch (mode) {
    case EditorGizmoMode::Rotate:
      return {
          EditorGizmoHandle::RotateX,
          EditorGizmoHandle::RotateY,
          EditorGizmoHandle::RotateZ,
      };
    case EditorGizmoMode::Scale:
      return {
          EditorGizmoHandle::ScaleX,
          EditorGizmoHandle::ScaleY,
          EditorGizmoHandle::ScaleZ,
      };
    case EditorGizmoMode::Translate:
    default:
      return {
          EditorGizmoHandle::TranslateX,
          EditorGizmoHandle::TranslateY,
          EditorGizmoHandle::TranslateZ,
      };
  }
}

inline CameraFrame make_camera_frame(
    const scene::Transform &transform,
    const rendering::Camera &camera
) {
  return CameraFrame{
      .position = transform.position,
      .forward = glm::length2(camera.front) > 0.0f
                     ? glm::normalize(camera.front)
                     : glm::vec3(0.0f, 0.0f, -1.0f),
      .up = glm::length2(camera.up) > 0.0f
                ? glm::normalize(camera.up)
                : glm::vec3(0.0f, 1.0f, 0.0f),
      .view = camera.view_matrix,
      .projection = camera.projection_matrix,
      .orthographic = camera.orthographic,
      .fov_degrees = camera.fov_degrees,
      .orthographic_scale = camera.orthographic_scale,
  };
}

inline glm::vec3 fallback_perpendicular(glm::vec3 axis) {
  const glm::vec3 reference =
      std::abs(axis.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f)
                              : glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 perpendicular = glm::cross(axis, reference);
  if (glm::length2(perpendicular) <= k_epsilon) {
    perpendicular = glm::cross(axis, glm::vec3(0.0f, 0.0f, 1.0f));
  }

  return glm::normalize(perpendicular);
}

inline std::pair<glm::vec3, glm::vec3> orthonormal_basis(glm::vec3 axis) {
  axis = glm::normalize(axis);
  const glm::vec3 tangent = fallback_perpendicular(axis);
  const glm::vec3 bitangent = glm::normalize(glm::cross(axis, tangent));
  return {tangent, bitangent};
}

inline float world_units_per_pixel(
    const CameraFrame &camera,
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

inline float gizmo_scale_world(
    const CameraFrame &camera,
    const glm::vec3 &pivot,
    float viewport_height
) {
  return world_units_per_pixel(camera, pivot, viewport_height) *
         k_target_pixel_size;
}

inline ProjectedPoint project_world_point(
    const CameraFrame &camera,
    const ui::UIRect &viewport_rect,
    const glm::vec3 &point
) {
  const glm::vec4 clip =
      camera.projection * camera.view * glm::vec4(point, 1.0f);
  if (clip.w <= k_epsilon) {
    return {};
  }

  const glm::vec3 ndc = glm::vec3(clip) / clip.w;
  return ProjectedPoint{
      .position =
          glm::vec2(
              viewport_rect.x + (ndc.x * 0.5f + 0.5f) * viewport_rect.width,
              viewport_rect.y +
                  (1.0f - (ndc.y * 0.5f + 0.5f)) * viewport_rect.height
          ),
      .valid = true,
  };
}

inline ScreenRay screen_ray(
    const CameraFrame &camera,
    const ui::UIRect &viewport_rect,
    glm::vec2 screen_point
) {
  const float ndc_x =
      ((screen_point.x - viewport_rect.x) / viewport_rect.width) * 2.0f - 1.0f;
  const float ndc_y =
      1.0f - ((screen_point.y - viewport_rect.y) / viewport_rect.height) * 2.0f;

  const glm::mat4 inverse_view_projection =
      glm::inverse(camera.projection * camera.view);

  glm::vec4 near_clip(ndc_x, ndc_y, -1.0f, 1.0f);
  glm::vec4 far_clip(ndc_x, ndc_y, 1.0f, 1.0f);

  glm::vec4 near_world = inverse_view_projection * near_clip;
  glm::vec4 far_world = inverse_view_projection * far_clip;
  if (std::abs(near_world.w) > k_epsilon) {
    near_world /= near_world.w;
  }
  if (std::abs(far_world.w) > k_epsilon) {
    far_world /= far_world.w;
  }

  if (camera.orthographic) {
    return ScreenRay{
        .origin = glm::vec3(near_world),
        .direction = glm::normalize(glm::vec3(far_world - near_world)),
    };
  }

  return ScreenRay{
      .origin = camera.position,
      .direction = glm::normalize(glm::vec3(far_world) - camera.position),
  };
}

inline std::optional<glm::vec3> intersect_ray_plane(
    const ScreenRay &ray,
    const glm::vec3 &plane_point,
    const glm::vec3 &plane_normal
) {
  const float denominator = glm::dot(ray.direction, plane_normal);
  if (std::abs(denominator) <= k_epsilon) {
    return std::nullopt;
  }

  const float distance =
      glm::dot(plane_point - ray.origin, plane_normal) / denominator;
  if (distance < 0.0f) {
    return std::nullopt;
  }

  return ray.origin + ray.direction * distance;
}

inline glm::vec3 translation_drag_plane_normal(
    const CameraFrame &camera,
    const glm::vec3 &axis,
    const glm::vec3 &pivot
) {
  glm::vec3 view_direction = camera.orthographic
                                 ? camera.forward
                                 : glm::normalize(pivot - camera.position);
  glm::vec3 plane_normal = view_direction - axis * glm::dot(view_direction, axis);
  if (glm::length2(plane_normal) <= k_epsilon) {
    plane_normal = camera.up - axis * glm::dot(camera.up, axis);
  }
  if (glm::length2(plane_normal) <= k_epsilon) {
    plane_normal = fallback_perpendicular(axis);
  }

  return glm::normalize(plane_normal);
}

inline float signed_angle_on_axis(
    const glm::vec3 &from,
    const glm::vec3 &to,
    const glm::vec3 &axis
) {
  const glm::vec3 normalized_from = glm::normalize(from);
  const glm::vec3 normalized_to = glm::normalize(to);
  const float dot_value =
      std::clamp(glm::dot(normalized_from, normalized_to), -1.0f, 1.0f);
  return std::atan2(
      glm::dot(axis, glm::cross(normalized_from, normalized_to)),
      dot_value
  );
}

inline float scale_factor_from_axis_delta(float axis_delta, float gizmo_scale) {
  return std::max(
      k_min_scale_component,
      1.0f + axis_delta / std::max(gizmo_scale, k_epsilon)
  );
}

inline float distance_to_segment(glm::vec2 point, glm::vec2 a, glm::vec2 b) {
  const glm::vec2 segment = b - a;
  const float length_squared = glm::dot(segment, segment);
  if (length_squared <= k_epsilon) {
    return glm::length(point - a);
  }

  const float t =
      std::clamp(glm::dot(point - a, segment) / length_squared, 0.0f, 1.0f);
  const glm::vec2 closest = a + segment * t;
  return glm::length(point - closest);
}

inline void append_segment(
    std::vector<GizmoLineSegment> &segments,
    EditorGizmoHandle handle,
    glm::vec3 start,
    glm::vec3 end
) {
  segments.push_back(GizmoLineSegment{
      .start = start,
      .end = end,
      .handle = handle,
  });
}

inline std::vector<GizmoLineSegment> build_line_segments(
    EditorGizmoMode mode,
    const glm::vec3 &pivot,
    float gizmo_scale
) {
  std::vector<GizmoLineSegment> segments;
  segments.reserve(320u);

  for (EditorGizmoHandle handle : handles_for_mode(mode)) {
    const glm::vec3 axis = axis_vector(handle);
    const auto [tangent, bitangent] = orthonormal_basis(axis);

    if (mode == EditorGizmoMode::Rotate) {
      const float radius = gizmo_scale * k_ring_radius_ratio;
      for (int index = 0; index < k_ring_segment_count; ++index) {
        const float angle0 =
            (glm::two_pi<float>() * static_cast<float>(index)) /
            static_cast<float>(k_ring_segment_count);
        const float angle1 =
            (glm::two_pi<float>() * static_cast<float>(index + 1)) /
            static_cast<float>(k_ring_segment_count);
        const glm::vec3 start =
            pivot + tangent * std::cos(angle0) * radius +
            bitangent * std::sin(angle0) * radius;
        const glm::vec3 end =
            pivot + tangent * std::cos(angle1) * radius +
            bitangent * std::sin(angle1) * radius;
        append_segment(segments, handle, start, end);
      }
      continue;
    }

    const glm::vec3 tip = pivot + axis * gizmo_scale;
    append_segment(segments, handle, pivot, tip);

    if (mode == EditorGizmoMode::Translate) {
      const float head_length = gizmo_scale * k_arrow_head_ratio;
      append_segment(
          segments,
          handle,
          tip,
          tip - axis * head_length + tangent * head_length * 0.55f
      );
      append_segment(
          segments,
          handle,
          tip,
          tip - axis * head_length - tangent * head_length * 0.55f
      );
      continue;
    }

    const float cap_size = gizmo_scale * k_scale_cap_ratio;
    append_segment(
        segments,
        handle,
        tip - tangent * cap_size,
        tip + tangent * cap_size
    );
    append_segment(
        segments,
        handle,
        tip - bitangent * cap_size,
        tip + bitangent * cap_size
    );
  }

  return segments;
}

struct GizmoMeshVertex {
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec4 color = glm::vec4(1.0f);
};

inline void append_cylinder(
    std::vector<GizmoMeshVertex> &vertices,
    const glm::vec3 &base,
    const glm::vec3 &tip,
    float radius,
    const glm::vec4 &color,
    int segments = k_mesh_circle_segments
) {
  const glm::vec3 axis = tip - base;
  if (glm::length2(axis) < k_epsilon) {
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

inline void append_cone(
    std::vector<GizmoMeshVertex> &vertices,
    const glm::vec3 &base,
    const glm::vec3 &tip,
    float radius,
    const glm::vec4 &color,
    int segments = k_mesh_circle_segments
) {
  const glm::vec3 axis = tip - base;
  const float length = glm::length(axis);
  if (length < k_epsilon) {
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

inline void append_cube(
    std::vector<GizmoMeshVertex> &vertices,
    const glm::vec3 &center,
    float half_size,
    const glm::vec4 &color
) {
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

    const glm::vec3 vertex_0 = face_center - right - up;
    const glm::vec3 vertex_1 = face_center + right - up;
    const glm::vec3 vertex_2 = face_center + right + up;
    const glm::vec3 vertex_3 = face_center - right + up;

    vertices.push_back({vertex_0, face.normal, color});
    vertices.push_back({vertex_1, face.normal, color});
    vertices.push_back({vertex_2, face.normal, color});

    vertices.push_back({vertex_0, face.normal, color});
    vertices.push_back({vertex_2, face.normal, color});
    vertices.push_back({vertex_3, face.normal, color});
  }
}

inline void append_torus(
    std::vector<GizmoMeshVertex> &vertices,
    const glm::vec3 &center,
    const glm::vec3 &axis,
    float major_radius,
    float minor_radius,
    const glm::vec4 &color,
    int ring_segments = k_ring_segment_count,
    int tube_segments = k_torus_tube_segments
) {
  const auto [tangent, bitangent] = orthonormal_basis(axis);

  for (int i = 0; i < ring_segments; ++i) {
    const float theta_current =
        glm::two_pi<float>() * static_cast<float>(i) /
        static_cast<float>(ring_segments);
    const float theta_next =
        glm::two_pi<float>() * static_cast<float>(i + 1) /
        static_cast<float>(ring_segments);

    const glm::vec3 ring_direction_current =
        tangent * std::cos(theta_current) +
        bitangent * std::sin(theta_current);
    const glm::vec3 ring_direction_next =
        tangent * std::cos(theta_next) +
        bitangent * std::sin(theta_next);

    const glm::vec3 ring_center_current =
        center + ring_direction_current * major_radius;
    const glm::vec3 ring_center_next =
        center + ring_direction_next * major_radius;

    for (int j = 0; j < tube_segments; ++j) {
      const float phi_current =
          glm::two_pi<float>() * static_cast<float>(j) /
          static_cast<float>(tube_segments);
      const float phi_next =
          glm::two_pi<float>() * static_cast<float>(j + 1) /
          static_cast<float>(tube_segments);

      auto tube_point = [&](const glm::vec3 &ring_center,
                            const glm::vec3 &ring_direction,
                            float phi) {
        const glm::vec3 tube_normal =
            ring_direction * std::cos(phi) + axis * std::sin(phi);
        return std::pair{ring_center + tube_normal * minor_radius, tube_normal};
      };

      auto [position_00, normal_00] =
          tube_point(ring_center_current, ring_direction_current, phi_current);
      auto [position_01, normal_01] =
          tube_point(ring_center_current, ring_direction_current, phi_next);
      auto [position_10, normal_10] =
          tube_point(ring_center_next, ring_direction_next, phi_current);
      auto [position_11, normal_11] =
          tube_point(ring_center_next, ring_direction_next, phi_next);

      vertices.push_back({position_00, normal_00, color});
      vertices.push_back({position_10, normal_10, color});
      vertices.push_back({position_11, normal_11, color});

      vertices.push_back({position_00, normal_00, color});
      vertices.push_back({position_11, normal_11, color});
      vertices.push_back({position_01, normal_01, color});
    }
  }
}

template <typename ColorFn>
inline std::vector<GizmoMeshVertex> build_gizmo_mesh(
    EditorGizmoMode mode,
    const glm::vec3 &pivot,
    float gizmo_scale,
    ColorFn &&color_fn
) {
  std::vector<GizmoMeshVertex> vertices;
  vertices.reserve(8192u);

  const float shaft_radius = gizmo_scale * k_shaft_radius_ratio;
  const float cone_radius = gizmo_scale * k_cone_radius_ratio;
  const float cone_length = gizmo_scale * k_arrow_head_ratio;
  const float cube_half = gizmo_scale * k_scale_cap_ratio * 0.5f;
  const float torus_major = gizmo_scale * k_ring_radius_ratio;
  const float torus_minor = gizmo_scale * k_torus_tube_radius_ratio;

  for (EditorGizmoHandle handle : handles_for_mode(mode)) {
    const glm::vec3 axis = axis_vector(handle);
    const glm::vec4 color = color_fn(handle);

    if (mode == EditorGizmoMode::Rotate) {
      append_torus(
          vertices, pivot, axis, torus_major, torus_minor, color
      );
      continue;
    }

    const glm::vec3 tip = pivot + axis * gizmo_scale;

    if (mode == EditorGizmoMode::Translate) {
      const glm::vec3 shaft_end = tip - axis * cone_length;
      append_cylinder(vertices, pivot, shaft_end, shaft_radius, color);
      append_cone(vertices, shaft_end, tip, cone_radius, color);
      continue;
    }

    append_cylinder(vertices, pivot, tip, shaft_radius, color);
    append_cube(vertices, tip, cube_half, color);
  }

  return vertices;
}

inline std::optional<EditorGizmoHandle> pick_handle(
    const std::vector<GizmoLineSegment> &segments,
    const CameraFrame &camera,
    const ui::UIRect &viewport_rect,
    glm::vec2 cursor
) {
  float best_distance = k_handle_pick_threshold_pixels;
  EditorGizmoHandle best_handle = EditorGizmoHandle::None;

  for (const GizmoLineSegment &segment : segments) {
    const ProjectedPoint start =
        project_world_point(camera, viewport_rect, segment.start);
    const ProjectedPoint end =
        project_world_point(camera, viewport_rect, segment.end);
    if (!start.valid || !end.valid) {
      continue;
    }

    const float distance =
        distance_to_segment(cursor, start.position, end.position);
    if (distance < best_distance) {
      best_distance = distance;
      best_handle = segment.handle;
    }
  }

  if (best_handle == EditorGizmoHandle::None) {
    return std::nullopt;
  }

  return best_handle;
}

} // namespace astralix::editor::gizmo
