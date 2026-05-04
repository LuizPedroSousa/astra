#include "mesh.hpp"
#include "glm/glm.hpp"
#include "mikktspace.h"
#include "iostream"
#include "numbers"

namespace astralix {

Mesh Mesh::plane(float size) {
  float half_length = size / 2.0f;

  std::vector<Vertex> vertices = {
      // Front face
      {glm::vec3(-half_length, -half_length, half_length),
       glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f)},
      {glm::vec3(half_length, -half_length, half_length),
       glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f)},
      {glm::vec3(half_length, half_length, half_length),
       glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},
      {glm::vec3(-half_length, half_length, half_length),
       glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f)}};

  std::vector<unsigned int> indices = {
      // Front face
      0, 1, 2, 2, 3, 0,
  };

  return Mesh(vertices, indices);
}

Mesh Mesh::cube(float size) {
  float half_length = size / 2.0f;

  std::vector<Vertex> vertices = {
      // Front face
      {glm::vec3(-half_length, -half_length, half_length),
       glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f)},
      {glm::vec3(half_length, -half_length, half_length),
       glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f)},
      {glm::vec3(half_length, half_length, half_length),
       glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},
      {glm::vec3(-half_length, half_length, half_length),
       glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f)},

      // Back face
      {glm::vec3(half_length, -half_length, -half_length),
       glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 0.0f)},
      {glm::vec3(-half_length, -half_length, -half_length),
       glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 0.0f)},
      {glm::vec3(-half_length, half_length, -half_length),
       glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 1.0f)},
      {glm::vec3(half_length, half_length, -half_length),
       glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 1.0f)},

      // Left face
      {glm::vec3(-half_length, -half_length, -half_length),
       glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
      {glm::vec3(-half_length, -half_length, half_length),
       glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f)},
      {glm::vec3(-half_length, half_length, half_length),
       glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f)},
      {glm::vec3(-half_length, half_length, -half_length),
       glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f)},

      // Right face
      {glm::vec3(half_length, -half_length, half_length),
       glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
      {glm::vec3(half_length, -half_length, -half_length),
       glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f)},
      {glm::vec3(half_length, half_length, -half_length),
       glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f)},
      {glm::vec3(half_length, half_length, half_length),
       glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f)},

      // Top face
      {glm::vec3(-half_length, half_length, half_length),
       glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
      {glm::vec3(half_length, half_length, half_length),
       glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},
      {glm::vec3(half_length, half_length, -half_length),
       glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)},
      {glm::vec3(-half_length, half_length, -half_length),
       glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},

      // Bottom face
      {glm::vec3(-half_length, -half_length, -half_length),
       glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
      {glm::vec3(half_length, -half_length, -half_length),
       glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},
      {glm::vec3(half_length, -half_length, half_length),
       glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 1.0f)},
      {glm::vec3(-half_length, -half_length, half_length),
       glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f)}};

  std::vector<unsigned int> indices = {// Front face
                                       0, 1, 2, 2, 3, 0,

                                       // Back face
                                       4, 5, 6, 6, 7, 4,

                                       // Left face
                                       8, 9, 10, 10, 11, 8,

                                       // Right face
                                       12, 13, 14, 14, 15, 12,

                                       // Top face
                                       16, 17, 18, 18, 19, 16,

                                       // Bottom face
                                       20, 21, 22, 22, 23, 20};

  return Mesh(vertices, indices);
}

Mesh Mesh::sphere() {

  const int segments = 32;
  const int rings = 16;
  const float radius = 0.5f;

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  const float pi = std::numbers::pi_v<float>;

  for (int ring = 0; ring <= rings; ++ring) {
    float phi = ring * pi / rings;
    float y = radius * std::cos(phi);

    for (int segment = 0; segment <= segments; ++segment) {
      float theta = segment * 2 * pi / segments;
      float x = radius * std::sin(phi) * std::cos(theta);
      float z = radius * std::sin(phi) * std::sin(theta);

      Vertex vertex;
      vertex.position = glm::vec3(x, y, z);
      vertex.normal = glm::normalize(vertex.position);
      vertex.texture_coordinates =
          glm::vec2(static_cast<float>(segment) / segments,
                    static_cast<float>(ring) / rings);

      vertices.push_back(vertex);
    }
  }

  for (int ring = 0; ring < rings; ++ring) {
    for (int segment = 0; segment < segments; ++segment) {
      int current_ring = ring * (segments + 1);
      int next_ring = (ring + 1) * (segments + 1);

      indices.push_back(current_ring + segment);
      indices.push_back(next_ring + segment);
      indices.push_back(next_ring + segment + 1);

      indices.push_back(current_ring + segment);
      indices.push_back(next_ring + segment + 1);
      indices.push_back(current_ring + segment + 1);
    }
  }

  return Mesh(vertices, indices);
}

Mesh Mesh::quad(float size) {
  std::vector<Vertex> vertices = {
      {glm::vec3(-size, size, 0.0f), glm::vec3(0.0f), glm::vec2(0.0f, 1.0f)},
      {glm::vec3(-size, -size, 0.0f), glm::vec3(0.0f), glm::vec2(0.0f, 0.0f)},
      {glm::vec3(size, -size, 0.0f), glm::vec3(0.0f), glm::vec2(1.0f, 0.0f)},
      {glm::vec3(size, size, 0.0f), glm::vec3(0.0f), glm::vec2(1.0f, 1.0f)},
  };

  std::vector<unsigned int> indices = {0, 1, 2, 2,
                                       3, 0

  };

  return Mesh(vertices, indices);
}

Mesh Mesh::capsule(float radius, float height, int segments, int rings) {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  const float pi = std::numbers::pi_v<float>;
  const float halfHeight = height * 0.5f;

  for (int ring = 0; ring <= rings; ++ring) {
    float phi = ring * pi / rings;
    float y = std::cos(phi);
    float ringRadius = std::sin(phi);

    for (int segment = 0; segment <= segments; ++segment) {
      float theta = segment * 2 * pi / segments;
      float x = std::cos(theta) * ringRadius;
      float z = std::sin(theta) * ringRadius;

      Vertex vertex;
      vertex.position =
          glm::vec3(x * radius, y * radius * halfHeight, z * radius);
      vertex.normal = glm::normalize(vertex.position);
      vertex.texture_coordinates =
          glm::vec2(static_cast<float>(segment) / segments,
                    static_cast<float>(ring) / rings);

      vertices.push_back(vertex);
    }
  }

  for (int ring = 0; ring < rings; ++ring) {
    for (int segment = 0; segment < segments; ++segment) {
      int currentRing = ring * (segments + 1);
      int nextRing = (ring + 1) * (segments + 1);

      indices.push_back(currentRing + segment);
      indices.push_back(nextRing + segment);
      indices.push_back(nextRing + segment + 1);

      indices.push_back(currentRing + segment);
      indices.push_back(nextRing + segment + 1);
      indices.push_back(currentRing + segment + 1);
    }
  }

  return Mesh(vertices, indices);
}

namespace {

struct MikkUserData {
  std::vector<Vertex> *vertices;
  std::vector<unsigned int> *indices;
};

int mikk_get_num_faces(const SMikkTSpaceContext *context) {
  auto *data = static_cast<MikkUserData *>(context->m_pUserData);
  return static_cast<int>(data->indices->size() / 3);
}

int mikk_get_num_vertices_of_face(const SMikkTSpaceContext *, const int) {
  return 3;
}

void mikk_get_position(const SMikkTSpaceContext *context, float output[], const int face, const int vertex) {
  auto *data = static_cast<MikkUserData *>(context->m_pUserData);
  unsigned int index = (*data->indices)[face * 3 + vertex];
  const glm::vec3 &position = (*data->vertices)[index].position;
  output[0] = position.x;
  output[1] = position.y;
  output[2] = position.z;
}

void mikk_get_normal(const SMikkTSpaceContext *context, float output[], const int face, const int vertex) {
  auto *data = static_cast<MikkUserData *>(context->m_pUserData);
  unsigned int index = (*data->indices)[face * 3 + vertex];
  const glm::vec3 &normal = (*data->vertices)[index].normal;
  output[0] = normal.x;
  output[1] = normal.y;
  output[2] = normal.z;
}

void mikk_get_tex_coord(const SMikkTSpaceContext *context, float output[], const int face, const int vertex) {
  auto *data = static_cast<MikkUserData *>(context->m_pUserData);
  unsigned int index = (*data->indices)[face * 3 + vertex];
  const glm::vec2 &uv = (*data->vertices)[index].texture_coordinates;
  output[0] = uv.x;
  output[1] = uv.y;
}

void mikk_set_tspace_basic(const SMikkTSpaceContext *context, const float tangent[], const float sign, const int face, const int vertex) {
  auto *data = static_cast<MikkUserData *>(context->m_pUserData);
  unsigned int index = (*data->indices)[face * 3 + vertex];
  (*data->vertices)[index].tangent = glm::vec3(tangent[0], tangent[1], tangent[2]);
  (*data->vertices)[index].bitangent_sign = sign;
}

} // namespace

void Mesh::calculate_tangents_mikktspace() {
  MikkUserData user_data{&vertices, &indices};

  SMikkTSpaceInterface interface_callbacks{};
  interface_callbacks.m_getNumFaces = mikk_get_num_faces;
  interface_callbacks.m_getNumVerticesOfFace = mikk_get_num_vertices_of_face;
  interface_callbacks.m_getPosition = mikk_get_position;
  interface_callbacks.m_getNormal = mikk_get_normal;
  interface_callbacks.m_getTexCoord = mikk_get_tex_coord;
  interface_callbacks.m_setTSpaceBasic = mikk_set_tspace_basic;
  interface_callbacks.m_setTSpace = nullptr;

  SMikkTSpaceContext context{};
  context.m_pInterface = &interface_callbacks;
  context.m_pUserData = &user_data;

  genTangSpaceDefault(&context);
}

} // namespace astralix
