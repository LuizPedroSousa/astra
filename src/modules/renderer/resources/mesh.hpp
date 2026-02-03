#pragma once
#include "glm/glm.hpp"
#include "guid.hpp"
#include "renderer-api.hpp"
#include "vector"
#include "vertex-array.hpp"
#include <cstddef>

namespace astralix {

class Mesh;

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 texture_coordinates;
  glm::vec3 tangent;
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

  RendererAPI::DrawPrimitive draw_type = RendererAPI::DrawPrimitive::TRIANGLES;

  Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices) {
    this->vertices = vertices;
    this->indices = indices;

    calculate_tanget_and_bitangents();

    id = generate_hash_id();
  };

  ~Mesh() = default;

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

  void calculate_tanget_and_bitangents() {
    for (size_t i = 0; i < indices.size(); i += 3) {
      unsigned int i1 = indices[i];
      unsigned int i2 = indices[i + 1];
      unsigned int i3 = indices[i + 2];

      const Vertex &v1 = vertices[i1];
      const Vertex &v2 = vertices[i2];
      const Vertex &v3 = vertices[i3];

      glm::vec3 edge1 = v2.position - v1.position;
      glm::vec3 edge2 = v3.position - v1.position;

      glm::vec2 deltaUV1 = v2.texture_coordinates - v1.texture_coordinates;
      glm::vec2 deltaUV2 = v3.texture_coordinates - v1.texture_coordinates;

      float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
      glm::vec3 tangent = (edge1 * deltaUV2.y - edge2 * deltaUV1.y) * f;
      glm::vec3 bitangent = (edge2 * deltaUV1.x - edge1 * deltaUV2.x) * f;

      vertices[i1].tangent += tangent;
      vertices[i2].tangent += tangent;
      vertices[i3].tangent += tangent;
    }

    for (auto &vertex : vertices) {
      vertex.tangent = glm::normalize(vertex.tangent);
    }
  }
};

} // namespace astralix
