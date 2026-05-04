#pragma once

#include "asset_binding.hpp"
#include "asset_record.hpp"

#include <filesystem>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace astralix {

struct AssetGraphConfig {
  std::filesystem::path project_root;
  std::filesystem::path project_resources_root;
  std::filesystem::path engine_assets_root;
};

class AssetGraph {
public:
  explicit AssetGraph(AssetGraphConfig config);

  void load_root_assets(std::span<const AssetBindingConfig> roots);

  const AssetRecord *find_by_public_id(std::string_view id) const;
  const AssetRecord *find_by_asset_path(const std::filesystem::path &path) const;
  const AssetRecord *find_by_asset_key(const std::string &key) const;
  std::filesystem::path to_absolute_path(const ResolvedAssetPath &path) const;
  std::span<const AssetRecord> records() const;
  std::span<const AssetRecord *const> topological_order() const;
  bool reload_asset(const std::string &asset_key);

private:
  void load_asset_recursive(
      const ResolvedAssetPath &asset_path,
      std::vector<std::string> &stack
  );
  ResolvedAssetPath resolve_reference(
      const std::optional<ResolvedAssetPath> &owner_asset_path,
      std::string_view reference
  ) const;
  std::string to_asset_key(const ResolvedAssetPath &path) const;
  void finalize_records();

  AssetGraphConfig m_config;
  std::vector<AssetRecord> m_records;
  std::vector<const AssetRecord *> m_topological_order;
  std::unordered_map<std::string, size_t> m_record_index_by_key;
  std::unordered_map<std::string, size_t> m_public_record_index_by_id;
  std::unordered_map<std::string, std::string> m_public_id_by_key;
  std::unordered_map<std::string, std::string> m_asset_key_by_public_id;
};

} // namespace astralix
