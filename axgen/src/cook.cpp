#include "cook.hpp"

#include "project-locator.hpp"
#include "project-serializer.hpp"
#include "assets/asset_cooker.hpp"

#include <filesystem>
#include <iostream>
#include <optional>

namespace axgen {
namespace {

std::optional<std::filesystem::path> resolve_manifest(const Options &options,
                                                      std::string *error) {
  std::optional<std::filesystem::path> explicit_manifest;
  if (!options.manifest_path.empty()) {
    explicit_manifest = std::filesystem::path(options.manifest_path);
  }

  auto search_root = options.root_path.empty()
                         ? std::filesystem::current_path()
                         : std::filesystem::absolute(options.root_path);

  return resolve_manifest_path(explicit_manifest, search_root, error);
}

} // namespace

RunContext run_cook_once(const Options &options) {
  RunContext context;

  std::string manifest_error;
  auto manifest_path = resolve_manifest(options, &manifest_error);
  if (!manifest_path) {
    std::cerr << "error: " << manifest_error << '\n';
    return context;
  }

  std::string deserialize_error;
  auto manifest =
      ProjectSerializer::deserialize(*manifest_path, &deserialize_error);
  if (!manifest) {
    std::cerr << "error: " << deserialize_error << '\n';
    return context;
  }

  astralix::AssetCooker cooker(astralix::AssetGraphConfig{
      .project_root = manifest->project_root.lexically_normal(),
      .project_resources_root =
          (manifest->project_root / manifest->resources_directory)
              .lexically_normal(),
      .engine_assets_root =
          std::filesystem::path(ASTRALIX_ENGINE_ASSETS_DIR).lexically_normal(),
  });

  const auto output_root =
      (manifest->project_root / ".astralix" / "cooked").lexically_normal();
  auto output = cooker.cook(manifest->asset_bindings, output_root);

  std::cout << "axgen cook-assets: " << output.manifest.assets.size()
            << " asset(s), " << output.cooked_artifact_count
            << " cooked artifact(s), manifest "
            << output.manifest_path.generic_string() << '\n';

  context.ok = true;
  return context;
}

} // namespace axgen
