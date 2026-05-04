#pragma once

#include "path.hpp"

#include <filesystem>
#include <string>

namespace astralix {

struct ResolvedAssetPath {
  BaseDirectory base_directory = BaseDirectory::Project;
  std::filesystem::path relative_path;
};

inline std::string format_asset_reference(const ResolvedAssetPath &path) {
  const auto normalized = path.relative_path.lexically_normal().generic_string();
  if (path.base_directory == BaseDirectory::Engine) {
    return "@engine/" + normalized;
  }

  return normalized;
}

inline Ref<Path> to_runtime_path(const ResolvedAssetPath &resolved) {
  return Path::create(
      resolved.relative_path.generic_string(),
      resolved.base_directory
  );
}

} // namespace astralix
