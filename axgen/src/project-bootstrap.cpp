#include "project-bootstrap.hpp"

#include "project-serializer.hpp"

#include <map>

namespace axgen {

namespace {

std::optional<std::string> canonical_shader_id(const ManifestPath &path,
                                               std::string *error) {
  std::string prefix;
  switch (path.base_directory) {
  case BaseDirectory::Engine:
    prefix = "engine/";
    break;
  case BaseDirectory::Project:
    prefix = "project/";
    break;
  default:
    if (error) {
      *error = "unsupported shader base directory";
    }
    return std::nullopt;
  }

  return prefix +
         std::filesystem::path(path.relative_path)
             .lexically_normal()
             .generic_string();
}

std::filesystem::path resolve_shader_path(
    const ProjectManifest &manifest, const ManifestPath &path,
    const std::filesystem::path &engine_assets_dir) {
  if (path.base_directory == BaseDirectory::Engine) {
    return (engine_assets_dir / path.relative_path).lexically_normal();
  }

  auto project_assets_dir = manifest.project_root;
  if (!manifest.resources_directory.empty()) {
    project_assets_dir /= manifest.resources_directory;
  }

  return (project_assets_dir / path.relative_path).lexically_normal();
}

} // namespace

std::optional<ProjectShaderDiscovery>
discover_project_shaders(const std::filesystem::path &manifest_path,
                         std::string *error) {
  auto manifest = ProjectSerializer::deserialize(manifest_path, error);
  if (!manifest) {
    return std::nullopt;
  }

  std::map<std::string, std::filesystem::path> shader_inputs;

  const std::filesystem::path engine_assets_dir(ASTRALIX_ENGINE_ASSETS_DIR);
  const std::filesystem::path engine_generated_root(
      ASTRALIX_ENGINE_GENERATED_ROOT);

  auto collect_shader =
      [&](const std::optional<ManifestPath> &path) -> bool {
    if (!path.has_value()) {
      return true;
    }

    auto resolved = resolve_shader_path(*manifest, *path, engine_assets_dir);
    if (resolved.extension() != ".axsl") {
      return true;
    }

    std::string canonical_id_error;
    auto canonical_id = canonical_shader_id(*path, &canonical_id_error);
    if (!canonical_id) {
      if (error) {
        *error = canonical_id_error;
      }
      return false;
    }

    auto [it, inserted] = shader_inputs.emplace(*canonical_id, resolved);
    if (!inserted && it->second != resolved) {
      if (error) {
        *error = "canonical shader id '" + *canonical_id +
                 "' resolves to multiple source files";
      }
      return false;
    }

    return true;
  };

  for (const auto &descriptor : manifest->shaders) {
    if (!collect_shader(descriptor.vertex_path) ||
        !collect_shader(descriptor.fragment_path) ||
        !collect_shader(descriptor.geometry_path) ||
        !collect_shader(descriptor.compute_path)) {
      return std::nullopt;
    }
  }

  ProjectShaderDiscovery discovery;
  discovery.manifest_path = manifest->manifest_path;
  discovery.project_root = manifest->project_root.lexically_normal();
  discovery.engine_root = engine_generated_root.lexically_normal();
  discovery.shaders.reserve(shader_inputs.size());

  for (const auto &[canonical_id, source_path] : shader_inputs) {
    discovery.shaders.push_back({canonical_id, source_path});
  }

  return discovery;
}

} // namespace axgen
