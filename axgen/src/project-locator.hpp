#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace axgen {

std::optional<std::filesystem::path>
find_project_manifest(const std::filesystem::path &start_dir,
                      std::string *error = nullptr);

std::optional<std::filesystem::path>
resolve_manifest_path(const std::optional<std::filesystem::path> &manifest_path,
                      const std::filesystem::path &cwd,
                      std::string *error = nullptr);

} // namespace axgen
