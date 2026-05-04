#include "resources/mesh.hpp"

#include <gtest/gtest.h>

namespace astralix {
namespace {

TEST(MeshTest, MikkTSpaceProducesCorrectTangentForSimpleTriangle) {
  std::vector<Vertex> vertices = {
      {.position = glm::vec3(0.0f, 0.0f, 0.0f),
       .normal = glm::vec3(0.0f, 0.0f, 1.0f),
       .texture_coordinates = glm::vec2(0.0f, 0.0f),
       .tangent = glm::vec3(27.0f, -9.0f, 3.0f)},
      {.position = glm::vec3(1.0f, 0.0f, 0.0f),
       .normal = glm::vec3(0.0f, 0.0f, 1.0f),
       .texture_coordinates = glm::vec2(1.0f, 0.0f),
       .tangent = glm::vec3(-4.0f, 11.0f, 2.0f)},
      {.position = glm::vec3(0.0f, 1.0f, 0.0f),
       .normal = glm::vec3(0.0f, 0.0f, 1.0f),
       .texture_coordinates = glm::vec2(0.0f, 1.0f),
       .tangent = glm::vec3(5.0f, 5.0f, 5.0f)},
  };

  Mesh mesh(std::move(vertices), {0, 1, 2});

  for (const auto &vertex : mesh.vertices) {
    EXPECT_NEAR(vertex.tangent.x, 1.0f, 1e-4f);
    EXPECT_NEAR(vertex.tangent.y, 0.0f, 1e-4f);
    EXPECT_NEAR(vertex.tangent.z, 0.0f, 1e-4f);
  }
}

TEST(MeshTest, MikkTSpaceHandlesDegenerateUvs) {
  std::vector<Vertex> vertices = {
      {.position = glm::vec3(0.0f, 0.0f, 0.0f),
       .normal = glm::vec3(0.0f, 0.0f, 1.0f),
       .texture_coordinates = glm::vec2(0.0f, 0.0f)},
      {.position = glm::vec3(1.0f, 0.0f, 0.0f),
       .normal = glm::vec3(0.0f, 0.0f, 1.0f),
       .texture_coordinates = glm::vec2(0.0f, 0.0f)},
      {.position = glm::vec3(0.0f, 1.0f, 0.0f),
       .normal = glm::vec3(0.0f, 0.0f, 1.0f),
       .texture_coordinates = glm::vec2(0.0f, 0.0f)},
  };

  Mesh mesh(std::move(vertices), {0, 1, 2});

  for (const auto &vertex : mesh.vertices) {
    float length = glm::length(vertex.tangent);
    EXPECT_TRUE(length < 1e-4f || std::abs(length - 1.0f) < 1e-4f);
  }
}

} // namespace
} // namespace astralix
