#include "resources/mesh.hpp"

#include <gtest/gtest.h>

namespace astralix {
namespace {

TEST(MeshTest, RecomputesTangentsFromGeometryInsteadOfAccumulatingGarbage) {
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

TEST(MeshTest, BuildsFallbackTangentWhenUvsAreDegenerate) {
  std::vector<Vertex> vertices = {
      {.position = glm::vec3(0.0f, 0.0f, 0.0f),
       .normal = glm::vec3(0.0f, 0.0f, 1.0f),
       .texture_coordinates = glm::vec2(0.0f, 0.0f),
       .tangent = glm::vec3(99.0f)},
      {.position = glm::vec3(1.0f, 0.0f, 0.0f),
       .normal = glm::vec3(0.0f, 0.0f, 1.0f),
       .texture_coordinates = glm::vec2(0.0f, 0.0f),
       .tangent = glm::vec3(-17.0f)},
      {.position = glm::vec3(0.0f, 1.0f, 0.0f),
       .normal = glm::vec3(0.0f, 0.0f, 1.0f),
       .texture_coordinates = glm::vec2(0.0f, 0.0f),
       .tangent = glm::vec3(13.0f)},
  };

  Mesh mesh(std::move(vertices), {0, 1, 2});

  for (const auto &vertex : mesh.vertices) {
    EXPECT_NEAR(glm::length(vertex.tangent), 1.0f, 1e-4f);
    EXPECT_NEAR(glm::dot(vertex.normal, vertex.tangent), 0.0f, 1e-4f);
  }
}

} // namespace
} // namespace astralix
