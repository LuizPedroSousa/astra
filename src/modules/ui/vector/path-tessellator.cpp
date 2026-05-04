#include "vector/path-tessellator.hpp"

#include "glm/geometric.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

namespace astralix::ui {
namespace {

constexpr float k_path_epsilon = 1.0e-4f;
constexpr float k_round_arc_step = std::numbers::pi_v<float> / 12.0f;
constexpr float k_miter_limit = 4.0f;
constexpr int k_max_curve_depth = 8;

struct FlattenedContour {
  std::vector<glm::vec2> points;
  bool closed = false;
};

struct StrokeJoinGeometry {
  glm::vec2 prev_left = glm::vec2(0.0f);
  glm::vec2 prev_right = glm::vec2(0.0f);
  glm::vec2 next_left = glm::vec2(0.0f);
  glm::vec2 next_right = glm::vec2(0.0f);
  bool add_round_join = false;
  bool add_bevel_join = false;
  glm::vec2 round_from = glm::vec2(0.0f);
  glm::vec2 round_to = glm::vec2(0.0f);
  bool round_clockwise = false;
  glm::vec2 bevel_a = glm::vec2(0.0f);
  glm::vec2 bevel_b = glm::vec2(0.0f);
};

float cross(const glm::vec2 &lhs, const glm::vec2 &rhs) {
  return lhs.x * rhs.y - lhs.y * rhs.x;
}

bool nearly_equal(
    const glm::vec2 &lhs,
    const glm::vec2 &rhs,
    float epsilon = k_path_epsilon
) {
  return glm::length(lhs - rhs) <= epsilon;
}

glm::vec2 normalized_or_zero(const glm::vec2 &value) {
  const float length = glm::length(value);
  if (length <= k_path_epsilon) {
    return glm::vec2(0.0f);
  }
  return value / length;
}

glm::vec2 perpendicular_left(const glm::vec2 &direction) {
  return glm::vec2(-direction.y, direction.x);
}

glm::vec2 resolve_join_point(
    const glm::vec2 &point,
    const glm::vec2 &normal_a,
    const glm::vec2 &normal_b,
    float half_width
) {
  glm::vec2 averaged = normalized_or_zero(normal_a + normal_b);
  if (glm::length(averaged) <= k_path_epsilon) {
    return point + normal_b * half_width;
  }

  const float dot_product = glm::dot(averaged, normal_b);
  if (std::abs(dot_product) <= k_path_epsilon) {
    return point + normal_b * half_width;
  }

  return point + averaged * (half_width / dot_product);
}

void append_triangle(
    std::vector<UIPolylineVertex> &vertices,
    const glm::vec2 &a,
    const glm::vec2 &b,
    const glm::vec2 &c,
    const glm::vec4 &color
) {
  vertices.push_back({a, color});
  vertices.push_back({b, color});
  vertices.push_back({c, color});
}

void append_quad(
    std::vector<UIPolylineVertex> &vertices,
    const glm::vec2 &left_a,
    const glm::vec2 &right_a,
    const glm::vec2 &left_b,
    const glm::vec2 &right_b,
    const glm::vec4 &color
) {
  append_triangle(vertices, left_a, right_a, left_b, color);
  append_triangle(vertices, left_b, right_a, right_b, color);
}

void append_arc_fan(
    std::vector<UIPolylineVertex> &vertices,
    const glm::vec2 &center,
    const glm::vec2 &from,
    const glm::vec2 &to,
    const glm::vec4 &color,
    bool clockwise
) {
  const float radius = glm::length(from);
  if (radius <= k_path_epsilon || glm::length(to) <= k_path_epsilon) {
    return;
  }

  float start_angle = std::atan2(from.y, from.x);
  float end_angle = std::atan2(to.y, to.x);
  float delta = end_angle - start_angle;

  if (clockwise) {
    while (delta >= 0.0f) {
      delta -= std::numbers::pi_v<float> * 2.0f;
    }
  } else {
    while (delta <= 0.0f) {
      delta += std::numbers::pi_v<float> * 2.0f;
    }
  }

  const float sweep = std::abs(delta);
  if (sweep <= k_path_epsilon) {
    return;
  }

  const int segments =
      std::max(3, static_cast<int>(std::ceil(sweep / k_round_arc_step)));

  for (int index = 0; index < segments; ++index) {
    const float t0 =
        static_cast<float>(index) / static_cast<float>(segments);
    const float t1 =
        static_cast<float>(index + 1) / static_cast<float>(segments);
    const float angle0 = start_angle + delta * t0;
    const float angle1 = start_angle + delta * t1;

    append_triangle(
        vertices,
        center,
        center + glm::vec2(std::cos(angle0), std::sin(angle0)) * radius,
        center + glm::vec2(std::cos(angle1), std::sin(angle1)) * radius,
        color
    );
  }
}

std::vector<glm::vec2> simplify_polygon(std::vector<glm::vec2> polygon) {
  std::vector<glm::vec2> simplified;
  simplified.reserve(polygon.size());

  for (const glm::vec2 &point : polygon) {
    if (!simplified.empty() && nearly_equal(simplified.back(), point)) {
      continue;
    }
    simplified.push_back(point);
  }

  if (simplified.size() > 1u &&
      nearly_equal(simplified.front(), simplified.back())) {
    simplified.pop_back();
  }

  if (simplified.size() < 3u) {
    return {};
  }

  bool changed = true;
  while (changed && simplified.size() >= 3u) {
    changed = false;
    for (size_t index = 0u; index < simplified.size(); ++index) {
      const glm::vec2 &previous =
          simplified[(index + simplified.size() - 1u) % simplified.size()];
      const glm::vec2 &current = simplified[index];
      const glm::vec2 &next =
          simplified[(index + 1u) % simplified.size()];

      const glm::vec2 ab = current - previous;
      const glm::vec2 bc = next - current;
      if (glm::length(ab) <= k_path_epsilon ||
          glm::length(bc) <= k_path_epsilon ||
          std::abs(cross(ab, bc)) <= k_path_epsilon) {
        simplified.erase(
            simplified.begin() + static_cast<std::ptrdiff_t>(index)
        );
        changed = true;
        break;
      }
    }
  }

  return simplified.size() >= 3u ? simplified : std::vector<glm::vec2>{};
}

std::vector<glm::vec2> simplify_polyline(std::vector<glm::vec2> points) {
  std::vector<glm::vec2> simplified;
  simplified.reserve(points.size());

  for (const glm::vec2 &point : points) {
    if (!simplified.empty() && nearly_equal(simplified.back(), point)) {
      continue;
    }
    simplified.push_back(point);
  }

  if (simplified.size() < 2u) {
    return {};
  }

  bool changed = true;
  while (changed && simplified.size() >= 3u) {
    changed = false;
    for (size_t index = 1u; index + 1u < simplified.size(); ++index) {
      const glm::vec2 &previous = simplified[index - 1u];
      const glm::vec2 &current = simplified[index];
      const glm::vec2 &next = simplified[index + 1u];

      const glm::vec2 ab = current - previous;
      const glm::vec2 bc = next - current;
      if (glm::length(ab) <= k_path_epsilon ||
          glm::length(bc) <= k_path_epsilon ||
          std::abs(cross(ab, bc)) <= k_path_epsilon) {
        simplified.erase(
            simplified.begin() + static_cast<std::ptrdiff_t>(index)
        );
        changed = true;
        break;
      }
    }
  }

  return simplified.size() >= 2u ? simplified : std::vector<glm::vec2>{};
}

float signed_area(const std::vector<glm::vec2> &polygon) {
  if (polygon.size() < 3u) {
    return 0.0f;
  }

  float area = 0.0f;
  for (size_t index = 0u; index < polygon.size(); ++index) {
    const glm::vec2 &current = polygon[index];
    const glm::vec2 &next = polygon[(index + 1u) % polygon.size()];
    area += (current.x * next.y) - (next.x * current.y);
  }

  return area * 0.5f;
}

bool is_clockwise(const std::vector<glm::vec2> &polygon) {
  return signed_area(polygon) < 0.0f;
}

bool point_in_polygon(
    const glm::vec2 &point,
    const std::vector<glm::vec2> &polygon
) {
  bool inside = false;

  for (size_t index = 0u, previous = polygon.size() - 1u;
       index < polygon.size();
       previous = index++) {
    const glm::vec2 &vertex = polygon[index];
    const glm::vec2 &prev = polygon[previous];

    const bool intersects =
        ((vertex.y > point.y) != (prev.y > point.y)) &&
        (point.x <
         (prev.x - vertex.x) * (point.y - vertex.y) /
                 (prev.y - vertex.y + k_path_epsilon) +
             vertex.x);
    if (intersects) {
      inside = !inside;
    }
  }

  return inside;
}

bool point_in_triangle(
    const glm::vec2 &point,
    const glm::vec2 &a,
    const glm::vec2 &b,
    const glm::vec2 &c
) {
  const glm::vec2 v0 = c - a;
  const glm::vec2 v1 = b - a;
  const glm::vec2 v2 = point - a;

  const float dot00 = glm::dot(v0, v0);
  const float dot01 = glm::dot(v0, v1);
  const float dot02 = glm::dot(v0, v2);
  const float dot11 = glm::dot(v1, v1);
  const float dot12 = glm::dot(v1, v2);

  const float denominator = dot00 * dot11 - dot01 * dot01;
  if (std::abs(denominator) <= k_path_epsilon) {
    return false;
  }

  const float inverse = 1.0f / denominator;
  const float u = (dot11 * dot02 - dot01 * dot12) * inverse;
  const float v = (dot00 * dot12 - dot01 * dot02) * inverse;
  return u >= -k_path_epsilon && v >= -k_path_epsilon &&
         (u + v) <= (1.0f + k_path_epsilon);
}

std::vector<glm::vec2> triangulate_simple_polygon(
    std::vector<glm::vec2> polygon
) {
  polygon = simplify_polygon(std::move(polygon));
  if (polygon.size() < 3u) {
    return {};
  }

  if (is_clockwise(polygon)) {
    std::reverse(polygon.begin(), polygon.end());
  }

  std::vector<size_t> indices(polygon.size());
  for (size_t index = 0u; index < polygon.size(); ++index) {
    indices[index] = index;
  }

  std::vector<glm::vec2> triangles;
  triangles.reserve((polygon.size() - 2u) * 3u);

  size_t guard = 0u;
  while (indices.size() > 3u && guard < polygon.size() * polygon.size()) {
    bool ear_found = false;
    for (size_t index = 0u; index < indices.size(); ++index) {
      const size_t prev_index =
          indices[(index + indices.size() - 1u) % indices.size()];
      const size_t current_index = indices[index];
      const size_t next_index =
          indices[(index + 1u) % indices.size()];

      const glm::vec2 &a = polygon[prev_index];
      const glm::vec2 &b = polygon[current_index];
      const glm::vec2 &c = polygon[next_index];

      if (cross(b - a, c - b) <= k_path_epsilon) {
        continue;
      }

      bool contains_vertex = false;
      for (size_t test = 0u; test < indices.size(); ++test) {
        const size_t vertex_index = indices[test];
        if (vertex_index == prev_index || vertex_index == current_index ||
            vertex_index == next_index) {
          continue;
        }

        if (point_in_triangle(polygon[vertex_index], a, b, c)) {
          contains_vertex = true;
          break;
        }
      }

      if (contains_vertex) {
        continue;
      }

      triangles.push_back(a);
      triangles.push_back(b);
      triangles.push_back(c);
      indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(index));
      ear_found = true;
      break;
    }

    if (!ear_found) {
      break;
    }

    ++guard;
  }

  if (indices.size() == 3u) {
    triangles.push_back(polygon[indices[0u]]);
    triangles.push_back(polygon[indices[1u]]);
    triangles.push_back(polygon[indices[2u]]);
  }

  return triangles;
}

size_t rightmost_vertex_index(const std::vector<glm::vec2> &polygon) {
  size_t index = 0u;
  for (size_t candidate = 1u; candidate < polygon.size(); ++candidate) {
    if (polygon[candidate].x > polygon[index].x ||
        (std::abs(polygon[candidate].x - polygon[index].x) <= k_path_epsilon &&
         polygon[candidate].y < polygon[index].y)) {
      index = candidate;
    }
  }
  return index;
}

size_t find_outer_bridge_vertex(
    const std::vector<glm::vec2> &outer,
    const glm::vec2 &hole_point
) {
  float best_intersection_x = std::numeric_limits<float>::infinity();
  size_t best_vertex = 0u;
  bool found = false;

  for (size_t index = 0u; index < outer.size(); ++index) {
    const glm::vec2 &a = outer[index];
    const glm::vec2 &b = outer[(index + 1u) % outer.size()];

    if ((hole_point.y < std::min(a.y, b.y)) ||
        (hole_point.y > std::max(a.y, b.y)) ||
        std::abs(a.y - b.y) <= k_path_epsilon) {
      continue;
    }

    const float ratio = (hole_point.y - a.y) / (b.y - a.y);
    const float intersection_x = a.x + ratio * (b.x - a.x);
    if (intersection_x <= hole_point.x + k_path_epsilon) {
      continue;
    }

    if (intersection_x < best_intersection_x) {
      best_intersection_x = intersection_x;
      best_vertex = a.x > b.x ? index : (index + 1u) % outer.size();
      found = true;
    }
  }

  if (!found) {
    best_vertex = rightmost_vertex_index(outer);
  }

  return best_vertex;
}

std::vector<glm::vec2> merge_hole_into_polygon(
    std::vector<glm::vec2> outer,
    std::vector<glm::vec2> hole
) {
  outer = simplify_polygon(std::move(outer));
  hole = simplify_polygon(std::move(hole));

  if (outer.empty() || hole.empty()) {
    return outer;
  }

  if (is_clockwise(outer)) {
    std::reverse(outer.begin(), outer.end());
  }
  if (!is_clockwise(hole)) {
    std::reverse(hole.begin(), hole.end());
  }

  const size_t hole_index = rightmost_vertex_index(hole);
  const size_t outer_index =
      find_outer_bridge_vertex(outer, hole[hole_index]);

  std::vector<glm::vec2> merged;
  merged.reserve(outer.size() + hole.size() + 2u);

  for (size_t index = 0u; index <= outer_index; ++index) {
    merged.push_back(outer[index]);
  }

  for (size_t offset = 0u; offset < hole.size(); ++offset) {
    merged.push_back(hole[(hole_index + offset) % hole.size()]);
  }
  merged.push_back(hole[hole_index]);
  merged.push_back(outer[outer_index]);

  for (size_t index = outer_index + 1u; index < outer.size(); ++index) {
    merged.push_back(outer[index]);
  }

  return simplify_polygon(std::move(merged));
}

std::vector<glm::vec2> triangulate_contours(
    const std::vector<FlattenedContour> &contours
) {
  struct PreparedContour {
    std::vector<glm::vec2> points;
    int parent = -1;
    std::vector<size_t> children;
  };

  std::vector<PreparedContour> prepared;
  prepared.reserve(contours.size());

  for (const FlattenedContour &contour : contours) {
    if (!contour.closed) {
      continue;
    }

    std::vector<glm::vec2> polygon = simplify_polygon(contour.points);
    if (polygon.size() >= 3u) {
      prepared.push_back({std::move(polygon)});
    }
  }

  if (prepared.empty()) {
    return {};
  }

  for (size_t index = 0u; index < prepared.size(); ++index) {
    float best_area = std::numeric_limits<float>::infinity();
    for (size_t candidate = 0u; candidate < prepared.size(); ++candidate) {
      if (candidate == index) {
        continue;
      }

      const float candidate_area =
          std::abs(signed_area(prepared[candidate].points));
      const float index_area =
          std::abs(signed_area(prepared[index].points));
      if (candidate_area <= index_area) {
        continue;
      }

      if (!point_in_polygon(
              prepared[index].points.front(), prepared[candidate].points
          )) {
        continue;
      }

      if (candidate_area < best_area) {
        best_area = candidate_area;
        prepared[index].parent = static_cast<int>(candidate);
      }
    }
  }

  for (size_t index = 0u; index < prepared.size(); ++index) {
    if (prepared[index].parent >= 0) {
      prepared[prepared[index].parent].children.push_back(index);
    }
  }

  std::vector<glm::vec2> triangles;
  std::function<void(size_t, size_t)> append_triangles =
      [&](size_t contour_index, size_t depth) {
        if (depth % 2u == 0u) {
          std::vector<glm::vec2> polygon = prepared[contour_index].points;
          for (size_t child_index : prepared[contour_index].children) {
            polygon = merge_hole_into_polygon(
                std::move(polygon), prepared[child_index].points
            );
          }

          auto contour_triangles =
              triangulate_simple_polygon(std::move(polygon));
          triangles.insert(
              triangles.end(),
              contour_triangles.begin(),
              contour_triangles.end()
          );
        }

        for (size_t child_index : prepared[contour_index].children) {
          append_triangles(child_index, depth + 1u);
        }
      };

  for (size_t index = 0u; index < prepared.size(); ++index) {
    if (prepared[index].parent < 0) {
      append_triangles(index, 0u);
    }
  }

  return triangles;
}

void append_cubic_curve(
    std::vector<glm::vec2> &points,
    const glm::vec2 &p0,
    const glm::vec2 &p1,
    const glm::vec2 &p2,
    const glm::vec2 &p3,
    float flatness_tolerance,
    int depth = 0
) {
  const float flatness =
      std::max(
          std::abs(cross(p1 - p0, p3 - p0)),
          std::abs(cross(p2 - p0, p3 - p0))
      );
  if (flatness <= flatness_tolerance || depth >= k_max_curve_depth) {
    points.push_back(p3);
    return;
  }

  const glm::vec2 p01 = (p0 + p1) * 0.5f;
  const glm::vec2 p12 = (p1 + p2) * 0.5f;
  const glm::vec2 p23 = (p2 + p3) * 0.5f;
  const glm::vec2 p012 = (p01 + p12) * 0.5f;
  const glm::vec2 p123 = (p12 + p23) * 0.5f;
  const glm::vec2 p0123 = (p012 + p123) * 0.5f;

  append_cubic_curve(points, p0, p01, p012, p0123, flatness_tolerance, depth + 1);
  append_cubic_curve(points, p0123, p123, p23, p3, flatness_tolerance, depth + 1);
}

std::vector<FlattenedContour> flatten_path(
    const UIPathCommand &command,
    const UIPathTessellationOptions &options
) {
  std::vector<FlattenedContour> contours;
  FlattenedContour current;
  glm::vec2 current_point(0.0f);
  glm::vec2 subpath_start(0.0f);
  bool has_current_point = false;

  const auto flush_current = [&] {
    if (!current.points.empty()) {
      contours.push_back(std::move(current));
      current = FlattenedContour{};
    }
  };

  for (const UIPathElement &element : command.elements) {
    switch (element.verb) {
      case UIPathVerb::MoveTo:
        flush_current();
        current.points.push_back(element.p0);
        current_point = element.p0;
        subpath_start = element.p0;
        has_current_point = true;
        break;

      case UIPathVerb::LineTo:
        if (!has_current_point) {
          current.points.push_back(element.p0);
          current_point = element.p0;
          subpath_start = element.p0;
          has_current_point = true;
          break;
        }

        if (current.points.empty()) {
          current.points.push_back(current_point);
        }
        current.points.push_back(element.p0);
        current_point = element.p0;
        break;

      case UIPathVerb::CubicTo:
        if (!has_current_point) {
          break;
        }

        if (current.points.empty()) {
          current.points.push_back(current_point);
        }

        append_cubic_curve(
            current.points,
            current_point,
            element.p0,
            element.p1,
            element.p2,
            options.curve_flatness
        );
        current_point = element.p2;
        break;

      case UIPathVerb::Close:
        if (current.points.size() >= 2u) {
          current.closed = true;
          current_point = subpath_start;
        }
        flush_current();
        has_current_point = false;
        break;
    }
  }

  flush_current();
  return contours;
}

void append_fill_triangles(
    std::vector<UIPolylineVertex> &vertices,
    const std::vector<FlattenedContour> &contours,
    const glm::vec4 &color
) {
  const auto triangles = triangulate_contours(contours);
  for (size_t index = 0u; index + 2u < triangles.size(); index += 3u) {
    append_triangle(
        vertices,
        triangles[index],
        triangles[index + 1u],
        triangles[index + 2u],
        color
    );
  }
}

StrokeJoinGeometry resolve_stroke_join(
    const std::vector<glm::vec2> &points,
    const std::vector<glm::vec2> &normals,
    size_t index,
    bool closed,
    float half_width,
    UIStrokeJoin line_join
) {
  StrokeJoinGeometry geometry;
  const glm::vec2 point = points[index];

  if (!closed && (index == 0u || index + 1u == points.size())) {
    const size_t segment_index = index == 0u ? 0u : normals.size() - 1u;
    const glm::vec2 normal = normals[segment_index];
    geometry.prev_left = point + normal * half_width;
    geometry.prev_right = point - normal * half_width;
    geometry.next_left = geometry.prev_left;
    geometry.next_right = geometry.prev_right;
    return geometry;
  }

  const size_t prev_segment_index =
      (index + normals.size() - 1u) % normals.size();
  const size_t next_segment_index = index % normals.size();
  const glm::vec2 prev_normal = normals[prev_segment_index];
  const glm::vec2 next_normal = normals[next_segment_index];

  geometry.prev_left = point + prev_normal * half_width;
  geometry.prev_right = point - prev_normal * half_width;
  geometry.next_left = point + next_normal * half_width;
  geometry.next_right = point - next_normal * half_width;

  const glm::vec2 prev_direction =
      normalized_or_zero(points[index] - points[(index + points.size() - 1u) % points.size()]);
  const glm::vec2 next_direction =
      normalized_or_zero(points[(index + 1u) % points.size()] - points[index]);
  const float turn = cross(prev_direction, next_direction);

  if (std::abs(turn) <= k_path_epsilon) {
    const glm::vec2 left_join =
        resolve_join_point(point, prev_normal, next_normal, half_width);
    const glm::vec2 right_join =
        resolve_join_point(point, -prev_normal, -next_normal, half_width);
    geometry.prev_left = left_join;
    geometry.next_left = left_join;
    geometry.prev_right = right_join;
    geometry.next_right = right_join;
    return geometry;
  }

  if (turn > 0.0f) {
    const glm::vec2 inner_right =
        resolve_join_point(point, -prev_normal, -next_normal, half_width);
    geometry.prev_right = inner_right;
    geometry.next_right = inner_right;

    if (line_join == UIStrokeJoin::Miter) {
      const glm::vec2 left_join =
          resolve_join_point(point, prev_normal, next_normal, half_width);
      if (glm::length(left_join - point) <= half_width * k_miter_limit) {
        geometry.prev_left = left_join;
        geometry.next_left = left_join;
      } else {
        geometry.add_bevel_join = true;
        geometry.bevel_a = point + prev_normal * half_width;
        geometry.bevel_b = point + next_normal * half_width;
      }
    } else if (line_join == UIStrokeJoin::Bevel) {
      geometry.add_bevel_join = true;
      geometry.bevel_a = point + prev_normal * half_width;
      geometry.bevel_b = point + next_normal * half_width;
    } else {
      geometry.add_round_join = true;
      geometry.round_from = prev_normal * half_width;
      geometry.round_to = next_normal * half_width;
      geometry.round_clockwise = false;
    }

    return geometry;
  }

  const glm::vec2 inner_left =
      resolve_join_point(point, prev_normal, next_normal, half_width);
  geometry.prev_left = inner_left;
  geometry.next_left = inner_left;

  if (line_join == UIStrokeJoin::Miter) {
    const glm::vec2 right_join =
        resolve_join_point(point, -prev_normal, -next_normal, half_width);
    if (glm::length(right_join - point) <= half_width * k_miter_limit) {
      geometry.prev_right = right_join;
      geometry.next_right = right_join;
    } else {
      geometry.add_bevel_join = true;
      geometry.bevel_a = point - prev_normal * half_width;
      geometry.bevel_b = point - next_normal * half_width;
    }
  } else if (line_join == UIStrokeJoin::Bevel) {
    geometry.add_bevel_join = true;
    geometry.bevel_a = point - prev_normal * half_width;
    geometry.bevel_b = point - next_normal * half_width;
  } else {
    geometry.add_round_join = true;
    geometry.round_from = -prev_normal * half_width;
    geometry.round_to = -next_normal * half_width;
    geometry.round_clockwise = true;
  }

  return geometry;
}

void append_stroke_triangles(
    std::vector<UIPolylineVertex> &vertices,
    std::vector<glm::vec2> points,
    bool closed,
    const UIPathStyle &style,
    const glm::vec4 &color
) {
  points = closed ? simplify_polygon(std::move(points))
                  : simplify_polyline(std::move(points));
  if (points.size() < 2u || style.stroke_width <= 0.0f) {
    return;
  }

  const float half_width = style.stroke_width * 0.5f;
  if (!closed && style.line_cap == UIStrokeCap::Square) {
    const glm::vec2 start_direction =
        normalized_or_zero(points[1u] - points[0u]);
    const glm::vec2 end_direction =
        normalized_or_zero(points.back() - points[points.size() - 2u]);
    points.front() -= start_direction * half_width;
    points.back() += end_direction * half_width;
  }

  const size_t segment_count = closed ? points.size() : points.size() - 1u;
  std::vector<glm::vec2> normals(segment_count);
  for (size_t index = 0u; index < segment_count; ++index) {
    const glm::vec2 direction = normalized_or_zero(
        points[(index + 1u) % points.size()] - points[index]
    );
    if (glm::length(direction) <= k_path_epsilon) {
      return;
    }
    normals[index] = perpendicular_left(direction);
  }

  std::vector<StrokeJoinGeometry> joins(points.size());
  for (size_t index = 0u; index < points.size(); ++index) {
    joins[index] = resolve_stroke_join(
        points, normals, index, closed, half_width, style.line_join
    );
  }

  for (size_t index = 0u; index < segment_count; ++index) {
    const size_t next_index = (index + 1u) % points.size();
    append_quad(
        vertices,
        joins[index].next_left,
        joins[index].next_right,
        joins[next_index].prev_left,
        joins[next_index].prev_right,
        color
    );
  }

  for (size_t index = 0u; index < points.size(); ++index) {
    const StrokeJoinGeometry &join = joins[index];
    if (join.add_bevel_join) {
      append_triangle(vertices, points[index], join.bevel_a, join.bevel_b, color);
    }
    if (join.add_round_join) {
      append_arc_fan(
          vertices,
          points[index],
          join.round_from,
          join.round_to,
          color,
          join.round_clockwise
      );
    }
  }

  if (!closed && style.line_cap == UIStrokeCap::Round) {
    const glm::vec2 start_direction =
        normalized_or_zero(points[1u] - points[0u]);
    const glm::vec2 end_direction =
        normalized_or_zero(points.back() - points[points.size() - 2u]);
    const glm::vec2 start_normal = perpendicular_left(start_direction);
    const glm::vec2 end_normal = perpendicular_left(end_direction);

    append_arc_fan(
        vertices,
        points.front(),
        -start_normal * half_width,
        start_normal * half_width,
        color,
        true
    );
    append_arc_fan(
        vertices,
        points.back(),
        end_normal * half_width,
        -end_normal * half_width,
        color,
        false
    );
  }
}

} // namespace

UITessellatedPath tessellate_path(
    const UIPathCommand &command,
    const UIPathTessellationOptions &options
) {
  UITessellatedPath tessellated;
  if (command.elements.empty()) {
    return tessellated;
  }

  const auto contours = flatten_path(command, options);
  if (contours.empty()) {
    return tessellated;
  }

  if (command.style.fill && command.style.fill_color.a > 0.0f) {
    append_fill_triangles(
        tessellated.triangle_vertices,
        contours,
        command.style.fill_color
    );
  }

  if (command.style.stroke && command.style.stroke_width > 0.0f &&
      command.style.stroke_color.a > 0.0f) {
    for (const FlattenedContour &contour : contours) {
      append_stroke_triangles(
          tessellated.triangle_vertices,
          contour.points,
          contour.closed,
          command.style,
          command.style.stroke_color
      );
    }
  }

  return tessellated;
}

} // namespace astralix::ui
