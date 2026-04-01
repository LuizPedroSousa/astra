#include "path-manager.hpp"
#include "assert.hpp"
#include "log.hpp"
#include "managers/project-manager.hpp"

namespace astralix {
constexpr std::string_view engineAssetsDirectory = ASTRALIX_ASSETS_DIR;

std::filesystem::path
PathManager::resolve_project_path(std::string relative_path) {
  auto project = ProjectManager::get()->get_active_project();

  auto project_config = project->get_config();
  auto resolved_path = std::filesystem::path(project_config.directory) /
                       project_config.resources.directory / relative_path;

  return resolved_path;
}

std::filesystem::path
PathManager::resolve_engine_path(std::string relative_path) {
  auto resolved_path =
      std::filesystem::path(engineAssetsDirectory) / relative_path;

  return resolved_path;
}

std::filesystem::path PathManager::resolve(Ref<Path> path) {

  switch (path->get_base_directory()) {
  case BaseDirectory::Project: {
    return resolve_project_path(path->get_relative_path());
  }

  case BaseDirectory::Engine: {
    return resolve_engine_path(path->get_relative_path());
  }

  default: {
    ASTRA_EXCEPTION(std::string("Can't resolve relative path basis"))
  }
  }
};

} // namespace astralix
