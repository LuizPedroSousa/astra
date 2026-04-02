#pragma once

#include "base-manager.hpp"
#include "base.hpp"
#include "entities/scene.hpp"
#include "project.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace astralix {

class SceneManager : public BaseManager<SceneManager> {
public:
  using SceneFactory = std::function<Ref<Scene>()>;

  SceneManager();

  template <typename T> void register_scene_type(std::string type) {
    register_scene_type(
        std::move(type),
        []() -> Ref<Scene> { return create_ref<T>(); }
    );
  }

  void register_scene_type(std::string type, SceneFactory factory);

  Scene *activate(std::string scene_id);
  Scene *get_active_scene();
  std::optional<std::string> get_active_scene_id();
  std::vector<ProjectSceneEntryConfig> get_scene_entries();
  const ProjectSceneEntryConfig *find_scene_entry(std::string_view scene_id);

  ~SceneManager() = default;

private:
  void ensure_project_state();
  void validate_project_scenes(const ProjectConfig &config) const;
  Ref<Scene> instantiate_scene(const ProjectSceneEntryConfig &entry);
  void register_console_commands();

  std::unordered_map<std::string, SceneFactory> m_scene_factories;
  std::unordered_map<std::string, Ref<Scene>> m_scene_instances;
  std::optional<uint64_t> m_loaded_project_id;
  std::optional<std::string> m_active_scene_id;
};

} // namespace astralix
