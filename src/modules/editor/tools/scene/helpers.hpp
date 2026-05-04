#pragma once

#include "editor-theme.hpp"
#include "managers/scene-manager.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace astralix::editor::scene_panel {

struct SceneMenuEntryPresentation {
  std::string status_text;
  glm::vec4 status_color = glm::vec4(0.0f);
};

struct ArtifactInfo {
  const char *label;
  std::string path;
  std::string size;
};

struct DerivedEntityInfo {
  std::string name;
  std::string detail;
  std::string state_text;
  glm::vec4 state_color = glm::vec4(0.0f);
};

struct SerializationSummary {
  std::string format;
  std::string components;
  std::string snapshots;
};

std::string lowercase_copy(std::string_view value);
bool scene_entry_matches_query(
    const ProjectSceneEntryConfig &entry,
    std::string_view query
);
const char *scene_session_kind_label(SceneSessionKind kind);
std::string scene_result_count_label(size_t count);
ResourceDescriptorID scene_menu_trigger_icon_texture(bool open);
bool scene_preview_artifact_exists(const ProjectSceneEntryConfig &entry);
bool scene_runtime_artifact_exists(const ProjectSceneEntryConfig &entry);

const char *scene_source_save_state_label(SceneSourceSaveState state);
const char *scene_preview_state_label(ScenePreviewState state);
const char *scene_runtime_state_label(SceneRuntimeState state);
const char *scene_execution_state_label(SceneExecutionState state);

glm::vec4 scene_source_save_state_color(
    const ScenePanelTheme &theme,
    SceneSourceSaveState state
);
glm::vec4 scene_preview_state_color(
    const ScenePanelTheme &theme,
    ScenePreviewState state
);
glm::vec4 scene_runtime_state_color(
    const ScenePanelTheme &theme,
    SceneRuntimeState state
);
glm::vec4 scene_execution_state_color(
    const ScenePanelTheme &theme,
    SceneExecutionState state
);

SceneMenuEntryPresentation describe_scene_menu_entry(
    const SceneLifecycleStatus &status,
    const ScenePanelTheme &theme
);
std::string scene_runtime_status_hint(
    const SceneLifecycleStatus &status,
    std::optional<SceneSessionKind> active_session_kind,
    std::optional<SceneExecutionState> active_execution_state
);
bool can_promote_from_active_session(
    bool has_active_scene,
    std::optional<SceneSessionKind> active_session_kind
);
std::string runtime_prompt_title(ScenePreviewState state);
std::string runtime_prompt_title(
    std::optional<SceneSessionKind> target_kind,
    const SceneLifecycleStatus &status
);
std::string runtime_prompt_body(
    std::optional<SceneSessionKind> current_kind,
    std::optional<SceneSessionKind> target_kind,
    const SceneLifecycleStatus &status
);

std::string format_file_size(uintmax_t bytes);
std::string scene_session_revision_label(const Scene *scene, SceneSessionKind kind);
std::string scene_artifact_activity_label(
    const ProjectSceneEntryConfig &entry,
    SceneSessionKind kind
);
std::vector<ArtifactInfo> gather_artifact_info(const ProjectSceneEntryConfig &entry);
std::vector<DerivedEntityInfo> gather_derived_entity_info(
    const Scene &scene,
    const ScenePanelTheme &theme
);
std::string derived_entity_summary(const Scene &scene);
SerializationSummary gather_serialization_summary(const Scene &scene);
const char *startup_target_label(SceneStartupTarget target);

std::string status_bar_warning(
    const SceneLifecycleStatus &status,
    const Scene *scene
);
std::string status_bar_hint(
    const SceneLifecycleStatus &status
);
bool status_bar_visible(const SceneLifecycleStatus &status);

} // namespace astralix::editor::scene_panel
