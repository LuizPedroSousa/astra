#pragma once
#include "glm/glm.hpp"
#include "guid.hpp"
#include "renderer-api.hpp"
#include "vector"
#include "vertex-array.hpp"
#include <cstddef>
#include <cmath>
#include <utility>

namespace astralix {

class Mesh;

struct AABB {
  glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
  glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());

  glm::vec3 center() const { return (min + max) * 0.5f; }
  glm::vec3 extents() const { return (max - min) * 0.5f; }
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 texture_coordinates;
  glm::vec3 tangent;
  float bitangent_sign = 1.0f;
};

class Mesh {
public:
  Ref<VertexArray> vertex_array;

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  MeshID id;

  static Mesh capsule(float radius = 0.5f, float height = 1.0f,
                      int segments = 16, int rings = 8);

  static Mesh cube(float size = 1.0f);
  static Mesh plane(float size = 2.0f);
  static Mesh quad(float size = 1.0f);
  static Mesh sphere();

  AABB bounds;

  RendererAPI::DrawPrimitive draw_type = RendererAPI::DrawPrimitive::TRIANGLES;

  Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices,
       bool = false) {
    this->vertices = std::move(vertices);
    this->indices = std::move(indices);

    calculate_tangents_mikktspace();

    compute_bounds();
    id = generate_hash_id();
  };

  ~Mesh() = default;

  void compute_bounds() {
    bounds = AABB{};
    for (const auto &vertex : vertices) {
      bounds.min = glm::min(bounds.min, vertex.position);
      bounds.max = glm::max(bounds.max, vertex.position);
    }
  }

  std::size_t generate_hash_id() {
    std::size_t seed = 0;

    for (const auto &vertex : vertices) {
      seed ^= std::hash<float>{}(vertex.position.x) + 0x9e3779b9 + (seed << 6) +
              (seed >> 2);
      seed ^= std::hash<float>{}(vertex.position.y) + 0x9e3779b9 + (seed << 6) +
              (seed >> 2);
      seed ^= std::hash<float>{}(vertex.position.z) + 0x9e3779b9 + (seed << 6) +
              (seed >> 2);
    }

    for (const auto &index : indices) {
      seed ^= std::hash<unsigned int>{}(index) + 0x9e3779b9 + (seed << 6) +
              (seed >> 2);
    }

    return seed;
  }

  void calculate_tangents_mikktspace();
};

} // namespace astralix
