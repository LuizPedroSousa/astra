#include "assets/asset_registry.hpp"

#include "assert.hpp"
#include "log.hpp"
#include "resources/audio-clip.hpp"
#include "resources/font.hpp"
#include "resources/material.hpp"
#include "resources/model.hpp"
#include "resources/svg.hpp"
#include "resources/terrain-recipe.hpp"
#include "resources/texture.hpp"

namespace astralix {
namespace {

TextureParameter texture_param_from_string(const std::string &key) {
  if (key == "wrap_s")
    return TextureParameter::WrapS;
  if (key == "wrap_t")
    return TextureParameter::WrapT;
  if (key == "mag_filter")
    return TextureParameter::MagFilter;
  if (key == "min_filter")
    return TextureParameter::MinFilter;

  ASTRA_EXCEPTION("Unknown texture parameter", key);
}

TextureValue texture_value_from_string(const std::string &value) {
  if (value == "repeat")
    return TextureValue::Repeat;
  if (value == "clamp_to_edge")
    return TextureValue::ClampToEdge;
  if (value == "clamp_to_border")
    return TextureValue::ClampToBorder;
  if (value == "linear")
    return TextureValue::Linear;
  if (value == "nearest")
    return TextureValue::Nearest;
  if (value == "linear_mip_map")
    return TextureValue::LinearMipMap;

  ASTRA_EXCEPTION("Unknown texture value", value);
}

ModelImportSettings to_runtime_import_settings(
    const ModelImportConfig &config
) {
  return ModelImportSettings{
      .triangulate = config.triangulate,
      .flip_uvs = config.flip_uvs,
      .generate_normals = config.generate_normals,
      .pre_transform_vertices = config.pre_transform_vertices,
  };
}

} // namespace

AssetRegistry::AssetRegistry(Ref<Project> project)
    : m_project(std::move(project)),
      m_graph(AssetGraphConfig{
          .project_root =
              std::filesystem::path(m_project->get_config().directory)
                  .lexically_normal(),
          .project_resources_root =
              (std::filesystem::path(m_project->get_config().directory) /
               m_project->get_config().resources.directory)
                  .lexically_normal(),
          .engine_assets_root = std::filesystem::path(ASTRALIX_ASSETS_DIR)
                                    .lexically_normal(),
      }),
      m_watcher(AssetWatcher::Config{.poll_interval = std::chrono::milliseconds(500)}) {}

AssetRegistry::~AssetRegistry() { m_watcher.stop(); }

static constexpr const char *MANIFEST_WATCH_KEY = "__manifest__";

void AssetRegistry::load_root_assets(std::span<const AssetBindingConfig> roots) {
  m_graph.load_root_assets(roots);
  register_records();
  start_watcher();
}

void AssetRegistry::reload_all(std::span<const AssetBindingConfig> roots) {
  LOG_INFO("AssetRegistry: reload_all triggered, releasing all records");

  m_watcher.stop();

  for (const auto &record : m_graph.records()) {
    release_record(record);
  }

  m_graph.load_root_assets(roots);
  register_records();
  start_watcher();

  LOG_INFO("AssetRegistry: reload_all complete,", m_graph.records().size(), "records");
}

void AssetRegistry::poll_reloads() {
  auto changed_keys = m_watcher.poll_changed();
  if (changed_keys.empty()) {
    return;
  }

  for (const auto &asset_key : changed_keys) {
    if (asset_key == MANIFEST_WATCH_KEY) {
      LOG_INFO("AssetWatcher: project manifest changed, reloading");
      m_project->reload_manifest();
      return;
    }

    const auto *record = m_graph.find_by_asset_key(asset_key);
    if (record == nullptr) {
      continue;
    }

    LOG_INFO("AssetWatcher: reloading", asset_key);

    release_record(*record);

    if (!m_graph.reload_asset(asset_key)) {
      LOG_ERROR("AssetWatcher: failed to reload", asset_key);
      continue;
    }

    const auto *updated_record = m_graph.find_by_asset_key(asset_key);
    if (updated_record == nullptr) {
      continue;
    }

    register_record(*updated_record);
    LOG_INFO("AssetWatcher: reloaded", asset_key);
  }
}

const AssetRecord *AssetRegistry::find_by_public_id(std::string_view id) const {
  return m_graph.find_by_public_id(id);
}

const AssetRecord *
AssetRegistry::find_by_asset_path(const std::filesystem::path &path) const {
  return m_graph.find_by_asset_path(path);
}

std::span<const AssetRecord> AssetRegistry::records() const {
  return m_graph.records();
}

std::optional<ResourceDescriptorID> AssetRegistry::dependency_descriptor_id(
    const AssetRecord &record,
    const std::optional<std::string> &asset_key
) const {
  if (!asset_key.has_value()) {
    return std::nullopt;
  }

  for (const auto &dependency : record.dependencies) {
    if (dependency.asset_key == *asset_key) {
      return dependency.descriptor_id;
    }
  }

  ASTRA_EXCEPTION(
      "Missing asset dependency for ",
      record.descriptor_id,
      ": ",
      *asset_key
  );
}

void AssetRegistry::register_records() {
  for (const auto *record : m_graph.topological_order()) {
    register_record(*record);
  }
}

void AssetRegistry::register_record(const AssetRecord &record) {
  LOG_INFO(
      "AssetRegistry: registering", record.descriptor_id,
      "(", asset_kind_name(record.kind), ")"
  );

  switch (record.kind) {
  case AssetKind::Texture2D:
    register_texture(record);
    break;
  case AssetKind::Material:
    register_material(record);
    break;
  case AssetKind::Model:
    register_model(record);
    break;
  case AssetKind::Font:
    register_font(record);
    break;
  case AssetKind::Svg:
    register_svg(record);
    break;
  case AssetKind::AudioClip:
    register_audio_clip(record);
    break;
  case AssetKind::TerrainRecipe:
    register_terrain_recipe(record);
    break;
  }
}

void AssetRegistry::release_record(const AssetRecord &record) {
  LOG_INFO(
      "AssetRegistry: releasing", record.descriptor_id,
      "(", asset_kind_name(record.kind), ")"
  );

  auto manager = resource_manager();

  switch (record.kind) {
  case AssetKind::Texture2D:
    manager->release_by_descriptor_id<Texture2DDescriptor>(record.descriptor_id);
    break;
  case AssetKind::Material:
    manager->release_by_descriptor_id<MaterialDescriptor>(record.descriptor_id);
    break;
  case AssetKind::Model:
    manager->release_by_descriptor_id<ModelDescriptor>(record.descriptor_id);
    break;
  case AssetKind::Font:
    manager->release_by_descriptor_id<FontDescriptor>(record.descriptor_id);
    break;
  case AssetKind::Svg:
    manager->release_by_descriptor_id<SvgDescriptor>(record.descriptor_id);
    break;
  case AssetKind::AudioClip:
    manager->release_descriptor_by_id<AudioClipDescriptor>(record.descriptor_id);
    break;
  case AssetKind::TerrainRecipe:
    manager->release_descriptor_by_id<TerrainRecipeDescriptor>(record.descriptor_id);
    break;
  }
}

void AssetRegistry::start_watcher() {
  m_watcher.clear();
  m_watcher.register_file(MANIFEST_WATCH_KEY, m_project->manifest_path());

  for (const auto &record : m_graph.records()) {
    m_watcher.register_file(record.asset_key, record.absolute_path);
  }

  m_watcher.start();
  LOG_INFO("AssetWatcher: tracking", m_graph.records().size(), "asset files + manifest");
}

void AssetRegistry::register_texture(const AssetRecord &record) {
  const auto &payload = std::get<TextureAssetData>(record.payload);
  std::unordered_map<TextureParameter, TextureValue> parameters;
  for (const auto &parameter : payload.parameters) {
    parameters.emplace(
        texture_param_from_string(parameter.key),
        texture_value_from_string(parameter.value)
    );
  }

  Texture2D::create(
      record.descriptor_id,
      to_runtime_path(payload.source_path),
      payload.flip,
      std::move(parameters)
  );
}

void AssetRegistry::register_material(const AssetRecord &record) {
  const auto &payload = std::get<MaterialAssetData>(record.payload);

  Material::create(
      record.descriptor_id,
      dependency_descriptor_id(record, payload.base_color_asset_key),
      dependency_descriptor_id(record, payload.normal_asset_key),
      dependency_descriptor_id(record, payload.metallic_asset_key),
      dependency_descriptor_id(record, payload.roughness_asset_key),
      dependency_descriptor_id(record, payload.metallic_roughness_asset_key),
      dependency_descriptor_id(record, payload.occlusion_asset_key),
      dependency_descriptor_id(record, payload.emissive_asset_key),
      dependency_descriptor_id(record, payload.displacement_asset_key),
      payload.base_color_factor,
      payload.emissive_factor,
      payload.metallic_factor,
      payload.roughness_factor,
      payload.occlusion_strength,
      payload.normal_scale,
      payload.height_scale,
      payload.bloom_intensity,
      payload.alpha_mask,
      payload.alpha_blend,
      payload.alpha_cutoff,
      payload.double_sided
  );
}

void AssetRegistry::register_model(const AssetRecord &record) {
  const auto &payload = std::get<ModelAssetData>(record.payload);

  std::vector<ResourceDescriptorID> material_ids;
  material_ids.reserve(payload.material_asset_keys.size());
  for (const auto &asset_key : payload.material_asset_keys) {
    auto material_id = dependency_descriptor_id(record, asset_key);
    ASTRA_ENSURE(
        !material_id.has_value(),
        "Model asset dependency is missing a material descriptor id: ",
        asset_key
    );
    material_ids.push_back(*material_id);
  }

  Model::create(
      record.descriptor_id,
      to_runtime_path(payload.source_path),
      to_runtime_import_settings(payload.import),
      std::move(material_ids)
  );
}

void AssetRegistry::register_font(const AssetRecord &record) {
  const auto &payload = std::get<SingleSourceAssetData>(record.payload);
  Font::create(record.descriptor_id, to_runtime_path(payload.source_path));
}

void AssetRegistry::register_svg(const AssetRecord &record) {
  const auto &payload = std::get<SingleSourceAssetData>(record.payload);
  Svg::create(record.descriptor_id, to_runtime_path(payload.source_path));
}

void AssetRegistry::register_audio_clip(const AssetRecord &record) {
  const auto &payload = std::get<SingleSourceAssetData>(record.payload);
  AudioClip::create(record.descriptor_id, to_runtime_path(payload.source_path));
}

void AssetRegistry::register_terrain_recipe(const AssetRecord &record) {
  TerrainRecipe::create(record.descriptor_id, to_runtime_path(record.asset_path));
}

} // namespace astralix
