#pragma once

#include "glm/glm.hpp"
#include "resources/mesh.hpp"
#include <array>

namespace astralix::rendering {

struct FrustumPlane {
  glm::vec3 normal;
  float distance;
};

struct Frustum {
  std::array<FrustumPlane, 6> planes;
};

inline Frustum extract_frustum(const glm::mat4 &view_projection) {
  Frustum frustum;

  const glm::mat4 &matrix = view_projection;

  frustum.planes[0].normal.x = matrix[0][3] + matrix[0][0];
  frustum.planes[0].normal.y = matrix[1][3] + matrix[1][0];
  frustum.planes[0].normal.z = matrix[2][3] + matrix[2][0];
  frustum.planes[0].distance = matrix[3][3] + matrix[3][0];

  frustum.planes[1].normal.x = matrix[0][3] - matrix[0][0];
  frustum.planes[1].normal.y = matrix[1][3] - matrix[1][0];
  frustum.planes[1].normal.z = matrix[2][3] - matrix[2][0];
  frustum.planes[1].distance = matrix[3][3] - matrix[3][0];

  frustum.planes[2].normal.x = matrix[0][3] + matrix[0][1];
  frustum.planes[2].normal.y = matrix[1][3] + matrix[1][1];
  frustum.planes[2].normal.z = matrix[2][3] + matrix[2][1];
  frustum.planes[2].distance = matrix[3][3] + matrix[3][1];

  frustum.planes[3].normal.x = matrix[0][3] - matrix[0][1];
  frustum.planes[3].normal.y = matrix[1][3] - matrix[1][1];
  frustum.planes[3].normal.z = matrix[2][3] - matrix[2][1];
  frustum.planes[3].distance = matrix[3][3] - matrix[3][1];

  frustum.planes[4].normal.x = matrix[0][3] + matrix[0][2];
  frustum.planes[4].normal.y = matrix[1][3] + matrix[1][2];
  frustum.planes[4].normal.z = matrix[2][3] + matrix[2][2];
  frustum.planes[4].distance = matrix[3][3] + matrix[3][2];

  frustum.planes[5].normal.x = matrix[0][3] - matrix[0][2];
  frustum.planes[5].normal.y = matrix[1][3] - matrix[1][2];
  frustum.planes[5].normal.z = matrix[2][3] - matrix[2][2];
  frustum.planes[5].distance = matrix[3][3] - matrix[3][2];

  for (auto &plane : frustum.planes) {
    float length = glm::length(plane.normal);
    plane.normal /= length;
    plane.distance /= length;
  }

  return frustum;
}

inline bool is_aabb_visible(const Frustum &frustum, const AABB &local_bounds, const glm::mat4 &model) {
  const glm::vec3 center = local_bounds.center();
  const glm::vec3 extents = local_bounds.extents();

  const glm::vec3 world_center = glm::vec3(model * glm::vec4(center, 1.0f));

  const glm::vec3 axis_x = glm::vec3(model[0]);
  const glm::vec3 axis_y = glm::vec3(model[1]);
  const glm::vec3 axis_z = glm::vec3(model[2]);

  for (const auto &plane : frustum.planes) {
    const float projected_extent =
        extents.x * std::abs(glm::dot(plane.normal, axis_x)) +
        extents.y * std::abs(glm::dot(plane.normal, axis_y)) +
        extents.z * std::abs(glm::dot(plane.normal, axis_z));

    const float signed_distance =
        glm::dot(plane.normal, world_center) + plane.distance;

    if (signed_distance + projected_extent < 0.0f) {
      return false;
    }
  }

  return true;
}

} // namespace astralix::rendering
