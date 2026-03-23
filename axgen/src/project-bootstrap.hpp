#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace axgen {

struct DiscoveredShaderInput {
  std::string canonical_id;
  std::filesystem::path source_path;
};

struct ProjectShaderDiscovery {
  std::filesystem::path manifest_path;
  std::filesystem::path project_root;
  std::filesystem::path engine_root;
  std::vector<DiscoveredShaderInput> shaders;
};

std::optional<ProjectShaderDiscovery>
discover_project_shaders(const std::filesystem::path &manifest_path,
                         std::string *error = nullptr);

} // namespace axgen
