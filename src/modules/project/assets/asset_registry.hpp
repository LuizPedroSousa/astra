#pragma once

#include "assets/asset_binding.hpp"
#include "assets/asset_graph.hpp"
#include "assets/asset_watcher.hpp"
#include "project.hpp"

#include <span>
#include <string_view>

namespace astralix {

class AssetRegistry {
public:
  explicit AssetRegistry(Ref<Project> project);
  ~AssetRegistry();

  void load_root_assets(std::span<const AssetBindingConfig> roots);
  void reload_all(std::span<const AssetBindingConfig> roots);
  void poll_reloads();

  const AssetRecord *find_by_public_id(std::string_view id) const;
  const AssetRecord *find_by_asset_path(const std::filesystem::path &path) const;
  std::span<const AssetRecord> records() const;

private:
  void register_records();
  void register_record(const AssetRecord &record);
  void release_record(const AssetRecord &record);
  void register_texture(const AssetRecord &record);
  void register_material(const AssetRecord &record);
  void register_model(const AssetRecord &record);
  void register_font(const AssetRecord &record);
  void register_svg(const AssetRecord &record);
  void register_audio_clip(const AssetRecord &record);
  void register_terrain_recipe(const AssetRecord &record);
  void start_watcher();
  std::optional<ResourceDescriptorID> dependency_descriptor_id(
      const AssetRecord &record,
      const std::optional<std::string> &asset_key
  ) const;

  Ref<Project> m_project;
  AssetGraph m_graph;
  AssetWatcher m_watcher;
};

} // namespace astralix
