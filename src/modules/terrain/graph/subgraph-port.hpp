#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <variant>
#include <vector>

namespace astralix::terrain {

enum class SubgraphOutputKind : uint8_t {
  Texture,
  Mesh,
  PlacementList,
};

struct TextureOutput {
  std::string id;
  uint32_t width = 0;
  uint32_t height = 0;
  const void *data = nullptr;
  uint32_t bytes_per_texel = 0;
};

struct MeshOutput {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> uvs;
  std::vector<uint32_t> indices;
  uint32_t lod_level = 0;
};

struct PlacementEntry {
  std::string model_id;
  glm::vec3 position = glm::vec3(0.0f);
  glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  glm::vec3 scale = glm::vec3(1.0f);
};

struct PlacementListOutput {
  std::vector<PlacementEntry> entries;
};

using SubgraphOutputData = std::variant<TextureOutput, MeshOutput, PlacementListOutput>;

struct SubgraphPort {
  std::string name;
  SubgraphOutputKind kind;
};

} // namespace astralix::terrain
