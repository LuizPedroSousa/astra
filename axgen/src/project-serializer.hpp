#pragma once

#include "assets/asset_binding.hpp"
#include "serialization-context.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace axgen {

enum class BaseDirectory : uint8_t { Unknown = 0, Engine = 1, Project = 2 };

struct ManifestPath {
  BaseDirectory base_directory = BaseDirectory::Project;
  std::string relative_path;
};

struct ShaderDescriptorInput {
  std::optional<ManifestPath> vertex_path;
  std::optional<ManifestPath> fragment_path;
  std::optional<ManifestPath> geometry_path;
  std::optional<ManifestPath> compute_path;
};

struct ProjectManifest {
  std::filesystem::path manifest_path;
  std::filesystem::path project_root;
  std::string resources_directory;
  astralix::SerializationFormat serialization_format =
      astralix::SerializationFormat::Json;
  std::vector<ShaderDescriptorInput> shaders;
  std::vector<astralix::AssetBindingConfig> asset_bindings;
};

class ProjectSerializer {
public:
  static std::optional<ProjectManifest>
  deserialize(const std::filesystem::path &manifest_path,
              std::string *error = nullptr);
};

} // namespace axgen
