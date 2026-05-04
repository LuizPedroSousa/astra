#pragma once

#include "entities/derived-override.hpp"
#include "project.hpp"
#include "world.hpp"

#include "guid.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace astralix {

class SceneSerializer;
class SceneBuildContext;
struct ProjectSceneEntryConfig;

enum class SceneArtifactKind {
  Source,
  Preview,
  Runtime,
};

enum class SceneSessionKind {
  Source,
  Preview,
  Runtime,
};

enum class SceneExecutionState {
  Static,
  Playing,
  Paused,
  Stopped,
};

struct ScenePreviewBuildInfo {
  uint64_t source_revision = 0u;
  std::string built_at_utc;
};

struct SceneRuntimePromotionInfo {
  uint64_t promoted_from_preview_revision = 0u;
  std::string promoted_at_utc;
};

class Scene {
public:
  Scene(std::string type);

  void update();

  const std::string &get_name() const {
    return m_scene_id.empty() ? m_scene_type : m_scene_id;
  }
  const std::string &get_scene_id() const { return m_scene_id; }
  const std::string &get_type() const { return m_scene_type; }
  const std::string &get_source_scene_path() const { return m_source_scene_path; }
  const std::string &get_preview_scene_path() const {
    return m_preview_scene_path;
  }
  const std::string &get_runtime_scene_path() const {
    return m_runtime_scene_path;
  }
  SceneSessionKind get_session_kind() const { return m_session_kind; }
  SceneExecutionState get_execution_state() const { return m_execution_state; }
  bool is_ready() const { return m_world_ready; }
  bool is_playing() const {
    return m_execution_state == SceneExecutionState::Playing;
  }
  bool supports_execution_controls() const {
    return m_session_kind == SceneSessionKind::Preview ||
           m_session_kind == SceneSessionKind::Runtime;
  }
  bool has_pending_reset() const { return m_pending_reset; }
  uint64_t get_session_revision() const { return m_session_revision; }
  bool has_saved_source_snapshot() const { return m_has_source_save_revision; }
  bool is_source_dirty() const {
    return !m_has_source_save_revision ||
           m_world.revision() != m_last_source_save_revision;
  }
  bool has_current_preview_build() const {
    return m_has_preview_build_revision &&
           m_world.revision() == m_last_preview_build_revision;
  }
  bool has_current_runtime_promotion() const {
    return m_has_runtime_promotion_revision &&
           m_last_runtime_promotion_revision == m_last_preview_build_revision;
  }

  void serialize();
  void save_source();
  void save_preview();
  void save_runtime();
  bool load_source();
  bool load_preview();
  bool load_runtime();
  void build_preview(bool persist_to_disk = true);
  bool promote_preview_to_runtime(bool persist_to_disk = true);
  bool reload_preview_if_changed();
  bool reload_runtime_if_changed();
  void play();
  void pause();
  bool stop();
  bool flush_pending_reset();

  const DerivedState &get_derived_state() const { return m_derived_state; }
  void set_derived_state(DerivedState state) {
    m_derived_state = std::move(state);
    if (m_session_kind == SceneSessionKind::Source && m_world_ready) {
      refresh_source_overlay();
    }
  }
  const SceneRenderOverrides &render_overrides() const {
    return m_render_overrides;
  }
  void set_render_overrides(SceneRenderOverrides overrides) {
    m_render_overrides = std::move(overrides);
    m_world.touch();
  }
  const std::optional<ScenePreviewBuildInfo> &get_preview_build_info() const {
    return m_preview_build_info;
  }
  void set_preview_build_info(std::optional<ScenePreviewBuildInfo> info) {
    m_preview_build_info = std::move(info);
  }
  const std::optional<SceneRuntimePromotionInfo> &
  get_runtime_promotion_info() const {
    return m_runtime_promotion_info;
  }
  void set_runtime_promotion_info(std::optional<SceneRuntimePromotionInfo> info) {
    m_runtime_promotion_info = std::move(info);
  }

  SceneSerializer *get_serializer() { return m_serializer.get(); };
  void set_serializer(Ref<SceneSerializer> scene_serializer) {
    m_serializer = scene_serializer;
  }

  ecs::World &world() { return m_world; }
  const ecs::World &world() const { return m_world; }
  SceneID get_id() const { return m_id; }

  ecs::EntityRef spawn_entity(std::string name, bool active = true) {
    return m_world.spawn(std::move(name), active);
  }
  ecs::EntityRef spawn_scene_entity(std::string name, bool active = true);

  ~Scene() {}

  friend class SceneManager;

protected:
  virtual void setup() {}
  virtual void update_source() {}
  virtual void update_preview() { update_runtime(); }
  virtual void update_runtime() {}
  virtual void build_source_world() {}
  virtual void after_source_ready() {}
  virtual void after_preview_ready() { after_runtime_ready(); }
  virtual void after_runtime_ready() {}
  virtual void evaluate_build(SceneBuildContext &ctx) { sync_meta(ctx); }
  virtual void sync_meta(SceneBuildContext &ctx) { (void)ctx; }

  Ref<SceneSerializer> m_serializer;
  SceneID m_id;
  ecs::World m_world;

private:
  void bind_to_manifest_entry(const ProjectSceneEntryConfig &entry);
  void ensure_setup();
  void mark_world_ready(bool ready) { m_world_ready = ready; }
  void set_session_kind(SceneSessionKind kind) {
    m_session_kind = kind;
    m_pending_reset = false;
    m_execution_state = kind == SceneSessionKind::Source
                            ? SceneExecutionState::Static
                            : SceneExecutionState::Stopped;
  }
  void capture_source_derived_state_from_overlay();
  void refresh_source_overlay();
  bool load_artifact(SceneArtifactKind artifact_kind);
  bool reload_artifact_if_changed(SceneArtifactKind artifact_kind);
  bool reload_current_session_from_serializer();
  void mark_session_reloaded();
  void remember_loaded_artifact(SceneArtifactKind artifact_kind);

  std::string m_scene_type;
  std::string m_scene_id;
  std::string m_source_scene_path;
  std::string m_preview_scene_path;
  std::string m_runtime_scene_path;
  SceneSessionKind m_session_kind = SceneSessionKind::Source;
  SceneExecutionState m_execution_state = SceneExecutionState::Static;
  bool m_setup_complete = false;
  bool m_world_ready = false;
  bool m_pending_reset = false;
  uint64_t m_session_revision = 0u;
  uint64_t m_last_source_save_revision = 0u;
  uint64_t m_last_preview_build_revision = 0u;
  uint64_t m_last_runtime_promotion_revision = 0u;
  bool m_has_source_save_revision = false;
  bool m_has_preview_build_revision = false;
  bool m_has_runtime_promotion_revision = false;
  std::optional<std::filesystem::file_time_type> m_loaded_preview_write_time;
  std::optional<std::filesystem::file_time_type> m_loaded_runtime_write_time;
  DerivedState m_derived_state;
  SceneRenderOverrides m_render_overrides;
  std::optional<ScenePreviewBuildInfo> m_preview_build_info;
  std::optional<SceneRuntimePromotionInfo> m_runtime_promotion_info;
};

} // namespace astralix
