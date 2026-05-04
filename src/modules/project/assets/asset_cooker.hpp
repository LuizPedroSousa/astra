#pragma once

#include "assets/asset_binding.hpp"
#include "assets/asset_graph.hpp"
#include "assets/pack_manifest.hpp"

#include <filesystem>
#include <span>

namespace astralix {

struct AssetCookOutput {
  PackManifest manifest;
  std::filesystem::path output_root;
  std::filesystem::path manifest_path;
  size_t cooked_artifact_count = 0;
};

class AssetCooker {
public:
  explicit AssetCooker(AssetGraphConfig config);

  AssetCookOutput cook(
      std::span<const AssetBindingConfig> roots,
      const std::filesystem::path &output_root
  );

private:
  AssetGraph m_graph;
};

} // namespace astralix
