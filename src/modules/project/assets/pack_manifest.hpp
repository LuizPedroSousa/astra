#pragma once

#include "assets/asset_kind.hpp"
#include "guid.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace astralix {

struct PackManifestAsset {
  ResourceDescriptorID descriptor_id;
  AssetKind kind = AssetKind::Texture2D;
  std::string source_asset;
  std::vector<ResourceDescriptorID> dependency_ids;
  std::vector<std::string> artifacts;
};

struct PackManifest {
  uint32_t version = 1;
  std::vector<PackManifestAsset> assets;

  void write(const std::filesystem::path &path) const;
};

} // namespace astralix
