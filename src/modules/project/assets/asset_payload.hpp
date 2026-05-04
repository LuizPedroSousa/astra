#pragma once

#include "asset_path.hpp"

#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace astralix {

struct TextureParameterEntry {
  std::string key;
  std::string value;
};

struct TextureAssetData {
  ResolvedAssetPath source_path;
  bool flip = false;
  std::vector<TextureParameterEntry> parameters;
};

struct MaterialAssetData {
  std::optional<std::string> base_color_asset_key;
  std::optional<std::string> normal_asset_key;
  std::optional<std::string> metallic_asset_key;
  std::optional<std::string> roughness_asset_key;
  std::optional<std::string> metallic_roughness_asset_key;
  std::optional<std::string> occlusion_asset_key;
  std::optional<std::string> emissive_asset_key;
  std::optional<std::string> displacement_asset_key;
  glm::vec4 base_color_factor = glm::vec4(1.0f);
  glm::vec3 emissive_factor = glm::vec3(0.0f);
  float metallic_factor = 1.0f;
  float roughness_factor = 1.0f;
  float occlusion_strength = 1.0f;
  float normal_scale = 1.0f;
  float height_scale = 0.0f;
  float bloom_intensity = 0.0f;
  bool alpha_mask = false;
  bool alpha_blend = false;
  float alpha_cutoff = 0.5f;
  bool double_sided = false;
};

struct ModelImportConfig {
  bool triangulate = true;
  bool flip_uvs = true;
  bool generate_normals = true;
  bool calculate_tangents = true;
  bool pre_transform_vertices = false;
};

struct ModelAssetData {
  ResolvedAssetPath source_path;
  ModelImportConfig import;
  std::vector<std::string> material_asset_keys;
};

struct SingleSourceAssetData {
  ResolvedAssetPath source_path;
};

struct TerrainAssetData {
  uint32_t version = 1;
};

using AssetPayload = std::variant<
    TextureAssetData,
    MaterialAssetData,
    ModelAssetData,
    SingleSourceAssetData,
    TerrainAssetData>;

} // namespace astralix
