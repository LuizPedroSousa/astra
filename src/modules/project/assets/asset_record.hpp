#pragma once

#include "asset_kind.hpp"
#include "asset_path.hpp"
#include "asset_payload.hpp"
#include "guid.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace astralix {

struct AssetDependency {
  ResolvedAssetPath asset_path;
  std::string asset_key;
  ResourceDescriptorID descriptor_id;
};

struct AssetRecord {
  AssetKind kind = AssetKind::Texture2D;
  ResolvedAssetPath asset_path;
  std::string asset_key;
  std::filesystem::path absolute_path;
  ResourceDescriptorID descriptor_id;
  std::optional<ResourceDescriptorID> public_id;
  std::vector<AssetDependency> dependencies;
  AssetPayload payload;
};

} // namespace astralix
