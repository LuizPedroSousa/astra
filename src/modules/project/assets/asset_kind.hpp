#pragma once

#include <cstdint>
#include <string_view>

namespace astralix {

enum class AssetKind : uint8_t {
  Texture2D,
  Material,
  Model,
  Font,
  Svg,
  AudioClip,
  TerrainRecipe,
};

inline std::string_view asset_kind_name(AssetKind kind) {
  switch (kind) {
  case AssetKind::Texture2D:
    return "texture2d";
  case AssetKind::Material:
    return "material";
  case AssetKind::Model:
    return "model";
  case AssetKind::Font:
    return "font";
  case AssetKind::Svg:
    return "svg";
  case AssetKind::AudioClip:
    return "audio";
  case AssetKind::TerrainRecipe:
    return "terrain";
  }

  return "unknown";
}

} // namespace astralix
