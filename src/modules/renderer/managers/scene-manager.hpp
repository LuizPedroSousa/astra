#pragma once

#include "base-manager.hpp"
#include "base.hpp"
#include "entities/scene.hpp"
#include "project.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace astralix {

enum class SceneSourceSaveState {
  Saved,
  Dirty,
};

enum class ScenePreviewState {
  Missing,
  Stale,
  Current,
  Error,
};

enum class SceneRuntimeState {
  Missing,
  BehindPreview,
  Promoted,
  Error,
};

struct SceneLifecycleStatus {
  SceneSourceSaveState source = SceneSourceSaveState::Saved;
  ScenePreviewState preview = ScenePreviewState::Missing;
  SceneRuntimeState runtime = SceneRuntimeState::Missing;
};

class SceneManager : public BaseManager<SceneManager> {
public:
  using SceneFactory = std::function<Ref<Scene>()>;

  SceneManager();

  template <typename T>
  void register_scene_type(std::string type) {
    register_scene_type(
        std::move(type),
        []() -> Ref<Scene> { return create_ref<T>(); }
    );
  }

  void register_scene_type(std::string type, SceneFactory factory);
  void unregister_scene_type(std::string_view type);
  void reset_scene_instances();
  void set_scene_activation_enabled(bool enabled);
  uint64_t scene_instance_generation() const;

  Scene *activate(std::string scene_id);
  Scene *activate_source(std::string scene_id);
  Scene *activate_preview(std::string scene_id);
  Scene *activate_runtime(std::string scene_id);
  Scene *get_active_scene();
  std::optional<std::string> get_active_scene_id();
  std::optional<SceneSessionKind> get_active_scene_session_kind();
  std::optional<SceneExecutionState> get_active_scene_execution_state();
  std::optional<SceneLifecycleStatus>
  get_scene_lifecycle_status(std::string_view scene_id);
  std::optional<SceneLifecycleStatus> get_active_scene_lifecycle_status();
  std::vector<ProjectSceneEntryConfig> get_scene_entries();
  const ProjectSceneEntryConfig *find_scene_entry(std::string_view scene_id);
  bool promote_source_to_preview(std::string scene_id);
  bool build_preview(std::string scene_id);
  bool promote_preview(std::string scene_id);
  bool play_active_scene();
  bool pause_active_scene();
  bool stop_active_scene();
  bool flush_pending_active_scene_state();

  ~SceneManager() = default;

private:
  struct ActiveSceneRef {
    std::string id;
    SceneSessionKind kind = SceneSessionKind::Source;
  };

  void clear_scene_state();
  void ensure_project_state();
  void validate_project_scenes(const ProjectConfig &config) const;
  Ref<Scene> instantiate_scene(
      const ProjectSceneEntryConfig &entry,
      SceneSessionKind kind,
      bool allow_missing_artifact = false
  );
  Scene *ensure_instance(
      std::string_view scene_id,
      SceneSessionKind kind,
      bool allow_missing_artifact = false
  );
  Scene *ensure_preview_session(std::string_view scene_id);
  Scene *ensure_runtime_session(std::string_view scene_id);
  bool sync_preview_session(std::string_view scene_id, bool persist_to_disk);
  bool sync_runtime_session(std::string_view scene_id, bool persist_to_disk);
  Scene *activate(std::string scene_id, SceneSessionKind kind);
  SceneSessionKind startup_session_kind(const ProjectConfig &config) const;
  void register_console_commands();

  std::unordered_map<std::string, SceneFactory> m_scene_factories;
  std::unordered_map<std::string, Ref<Scene>> m_source_scene_instances;
  std::unordered_map<std::string, Ref<Scene>> m_preview_scene_instances;
  std::unordered_map<std::string, Ref<Scene>> m_runtime_scene_instances;
  std::optional<uint64_t> m_loaded_project_id;
  std::optional<ActiveSceneRef> m_active_scene;
  bool m_scene_activation_enabled = true;
  uint64_t m_scene_instance_generation = 1u;
};

} // namespace astralix
