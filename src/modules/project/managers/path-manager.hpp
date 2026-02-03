#pragma once
#include "base-manager.hpp"
#include "path.hpp"
#include <filesystem>
#include <string>

namespace astralix {

class PathManager : public BaseManager<PathManager> {
public:
  PathManager() = default;

  std::filesystem::path resolve(Ref<Path> path);

private:
  std::filesystem::path resolve_project_path(std::string relative_path);
  std::filesystem::path resolve_engine_path(std::string relative_path);
};

inline const Ref<PathManager> path_manager() noexcept {
  return PathManager::get();
}
} // namespace astralix
