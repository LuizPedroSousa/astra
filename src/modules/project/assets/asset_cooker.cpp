#include "assets/asset_cooker.hpp"

#include "assert.hpp"
#include "assets/asset_path.hpp"
#include "entities/serializers/axmesh-serializer.hpp"
#include "importers/model-importer.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace astralix {
namespace {

uint64_t fnv1a_64(std::string_view input) {
  uint64_t hash = 14695981039346656037ull;
  for (unsigned char ch : input) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= 1099511628211ull;
  }
  return hash;
}

std::string hex_suffix(uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::nouppercase << std::setw(8) << std::setfill('0')
      << static_cast<uint32_t>(value & 0xffffffffu);
  return out.str();
}

std::string sanitize_stem(const std::filesystem::path &path) {
  std::string stem = path.stem().string();
  for (char &ch : stem) {
    if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-') {
      ch = '-';
    }
  }
  if (stem.empty()) {
    stem = "asset";
  }
  return stem;
}

std::string to_manifest_asset_path(const ResolvedAssetPath &path) {
  return format_asset_reference(path);
}

} // namespace

AssetCooker::AssetCooker(AssetGraphConfig config) : m_graph(std::move(config)) {}

AssetCookOutput AssetCooker::cook(
    std::span<const AssetBindingConfig> roots,
    const std::filesystem::path &output_root
) {
  m_graph.load_root_assets(roots);

  AssetCookOutput output;
  output.output_root = output_root.lexically_normal();
  output.manifest_path = (output.output_root / "pack.axpack").lexically_normal();
  output.manifest.assets.reserve(m_graph.records().size());

  const auto artifact_models_root =
      (output.output_root / "artifacts" / "models").lexically_normal();
  std::filesystem::create_directories(artifact_models_root);

  for (const auto *record : m_graph.topological_order()) {
    PackManifestAsset manifest_asset{
        .descriptor_id = record->descriptor_id,
        .kind = record->kind,
        .source_asset = to_manifest_asset_path(record->asset_path),
    };

    manifest_asset.dependency_ids.reserve(record->dependencies.size());
    for (const auto &dependency : record->dependencies) {
      manifest_asset.dependency_ids.push_back(dependency.descriptor_id);
    }

    if (record->kind == AssetKind::Model) {
      const auto &payload = std::get<ModelAssetData>(record->payload);
      auto imported = import_model_file(
          m_graph.to_absolute_path(payload.source_path),
          ModelImportSettings{
              .triangulate = payload.import.triangulate,
              .flip_uvs = payload.import.flip_uvs,
              .generate_normals = payload.import.generate_normals,
          }
      );

      ASTRA_ENSURE(
          imported.material_slot_count != payload.material_asset_keys.size(),
          "Model asset '",
          record->descriptor_id,
          "' declares ",
          payload.material_asset_keys.size(),
          " material asset(s) but importer found ",
          imported.material_slot_count,
          " material slot(s)"
      );

      const auto artifact_name =
          sanitize_stem(record->asset_path.relative_path) + "-" +
          hex_suffix(fnv1a_64(record->asset_key)) + ".axmesh";
      const auto artifact_relative =
          (std::filesystem::path("artifacts") / "models" / artifact_name)
              .generic_string();
      const auto artifact_absolute =
          (output.output_root / artifact_relative).lexically_normal();

      AxMeshSerializer::write(artifact_absolute, imported.meshes);
      manifest_asset.artifacts.push_back(artifact_relative);
      ++output.cooked_artifact_count;
    }

    output.manifest.assets.push_back(std::move(manifest_asset));
  }

  output.manifest.write(output.manifest_path);
  return output;
}

} // namespace astralix
