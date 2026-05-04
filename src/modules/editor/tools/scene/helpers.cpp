#include "tools/scene/helpers.hpp"

#include "entities/serializers/scene-snapshot.hpp"
#include "managers/project-manager.hpp"
#include "tools/inspector/fields.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>

namespace astralix::editor::scene_panel {
namespace {

glm::vec4 danger_color() {
  return glm::vec4(0.937f, 0.267f, 0.267f, 1.0f);
}

std::string join_labels(const std::vector<std::string> &values) {
  std::string joined;
  for (size_t index = 0u; index < values.size(); ++index) {
    if (index > 0u) {
      joined += ", ";
    }
    joined += values[index];
  }
  return joined;
}

const std::string &artifact_path_for_kind(
    const ProjectSceneEntryConfig &entry,
    SceneSessionKind kind
) {
  switch (kind) {
    case SceneSessionKind::Source:
      return entry.source_path;
    case SceneSessionKind::Preview:
      return entry.preview_path;
    case SceneSessionKind::Runtime:
      return entry.runtime_path;
  }

  return entry.source_path;
}

std::string relative_age_label(
    const std::optional<std::filesystem::file_time_type> &write_time
) {
  if (!write_time.has_value()) {
    return {};
  }

  const auto now = std::filesystem::file_time_type::clock::now();
  auto age = now - *write_time;
  if (age <= decltype(age)::zero()) {
    return "just now";
  }

  const auto minutes =
      std::chrono::duration_cast<std::chrono::minutes>(age).count();
  if (minutes <= 0) {
    return "just now";
  }

  if (minutes < 60) {
    return std::to_string(minutes) + "m ago";
  }

  const auto hours = minutes / 60;
  if (hours < 24) {
    return std::to_string(hours) + "h ago";
  }

  const auto days = hours / 24;
  return std::to_string(days) + "d ago";
}

std::string artifact_activity_prefix(SceneSessionKind kind) {
  switch (kind) {
    case SceneSessionKind::Source:
      return "last saved ";
    case SceneSessionKind::Preview:
      return "built ";
    case SceneSessionKind::Runtime:
      return "promoted ";
  }

  return {};
}

std::string serialization_format_label(SerializationFormat format) {
  switch (format) {
    case SerializationFormat::Json:
      return "JSON";
    case SerializationFormat::Toml:
      return "TOML";
    case SerializationFormat::Yaml:
      return "YAML";
    case SerializationFormat::Xml:
      return "XML";
  }

  return "JSON";
}

std::string derived_override_detail(const DerivedOverrideRecord &record) {
  std::vector<std::string> labels;
  labels.reserve(record.components.size() + record.removed_components.size());

  for (const auto &component : record.components) {
    labels.push_back(component.name);
  }

  for (const auto &removed_component : record.removed_components) {
    labels.push_back("Removed " + removed_component);
  }

  if (!labels.empty()) {
    return join_labels(labels);
  }

  if (record.name.has_value()) {
    return "Renamed to " + *record.name;
  }

  if (!record.active) {
    return "Entity disabled in derived output";
  }

  return "Entity modified in derived output";
}

std::string revision_gap_label(uint64_t gap, std::string_view target) {
  return std::string(target) + " is " + std::to_string(gap) +
         (gap == 1u ? " revision" : " revisions");
}

} // namespace

std::string lowercase_copy(std::string_view value) {
  std::string lowered(value);
  std::transform(
      lowered.begin(),
      lowered.end(),
      lowered.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
  );
  return lowered;
}

bool scene_entry_matches_query(
    const ProjectSceneEntryConfig &entry,
    std::string_view query
) {
  if (query.empty()) {
    return true;
  }

  const std::string needle = lowercase_copy(query);
  const auto contains_query = [&needle](std::string_view value) {
    return lowercase_copy(value).find(needle) != std::string::npos;
  };

  return contains_query(entry.id) || contains_query(entry.type) ||
         contains_query(entry.source_path) || contains_query(entry.preview_path) ||
         contains_query(entry.runtime_path);
}

const char *scene_session_kind_label(SceneSessionKind kind) {
  switch (kind) {
    case SceneSessionKind::Source:
      return "source";
    case SceneSessionKind::Preview:
      return "preview";
    case SceneSessionKind::Runtime:
      return "runtime";
  }

  return "scene";
}

std::string scene_result_count_label(size_t count) {
  return std::to_string(count) + (count == 1u ? " result" : " results");
}

ResourceDescriptorID scene_menu_trigger_icon_texture(bool open) {
  return open ? "icons::right_arrow_down" : "icons::right_arrow";
}

bool scene_preview_artifact_exists(const ProjectSceneEntryConfig &entry) {
  auto project = active_project();
  if (project == nullptr || entry.preview_path.empty()) {
    return false;
  }

  return std::filesystem::exists(project->resolve_path(entry.preview_path));
}

bool scene_runtime_artifact_exists(const ProjectSceneEntryConfig &entry) {
  auto project = active_project();
  if (project == nullptr || entry.runtime_path.empty()) {
    return false;
  }

  return std::filesystem::exists(project->resolve_path(entry.runtime_path));
}

const char *scene_source_save_state_label(SceneSourceSaveState state) {
  switch (state) {
    case SceneSourceSaveState::Saved:
      return "Saved";
    case SceneSourceSaveState::Dirty:
      return "Dirty";
  }

  return "Unknown";
}

const char *scene_preview_state_label(ScenePreviewState state) {
  switch (state) {
    case ScenePreviewState::Missing:
      return "Missing";
    case ScenePreviewState::Stale:
      return "Stale";
    case ScenePreviewState::Current:
      return "Current";
    case ScenePreviewState::Error:
      return "Error";
  }

  return "Unknown";
}

const char *scene_runtime_state_label(SceneRuntimeState state) {
  switch (state) {
    case SceneRuntimeState::Missing:
      return "Missing";
    case SceneRuntimeState::BehindPreview:
      return "Behind";
    case SceneRuntimeState::Promoted:
      return "Promoted";
    case SceneRuntimeState::Error:
      return "Error";
  }

  return "Unknown";
}

const char *scene_execution_state_label(SceneExecutionState state) {
  switch (state) {
    case SceneExecutionState::Static:
      return "Static";
    case SceneExecutionState::Playing:
      return "Playing";
    case SceneExecutionState::Paused:
      return "Paused";
    case SceneExecutionState::Stopped:
      return "Stopped";
  }

  return "Unknown";
}

glm::vec4 scene_source_save_state_color(
    const ScenePanelTheme &theme,
    SceneSourceSaveState state
) {
  return state == SceneSourceSaveState::Saved ? theme.success : theme.accent;
}

glm::vec4 scene_preview_state_color(
    const ScenePanelTheme &theme,
    ScenePreviewState state
) {
  switch (state) {
    case ScenePreviewState::Missing:
    case ScenePreviewState::Stale:
      return theme.accent;
    case ScenePreviewState::Current:
      return theme.success;
    case ScenePreviewState::Error:
      return danger_color();
  }

  return theme.accent;
}

glm::vec4 scene_runtime_state_color(
    const ScenePanelTheme &theme,
    SceneRuntimeState state
) {
  switch (state) {
    case SceneRuntimeState::Missing:
      return theme.accent;
    case SceneRuntimeState::BehindPreview:
    case SceneRuntimeState::Error:
      return danger_color();
    case SceneRuntimeState::Promoted:
      return theme.success;
  }

  return theme.accent;
}

glm::vec4 scene_execution_state_color(
    const ScenePanelTheme &theme,
    SceneExecutionState state
) {
  switch (state) {
    case SceneExecutionState::Playing:
      return theme.success;
    case SceneExecutionState::Paused:
      return theme.accent;
    case SceneExecutionState::Static:
    case SceneExecutionState::Stopped:
      return theme.text_muted;
  }

  return theme.text_muted;
}

SceneMenuEntryPresentation describe_scene_menu_entry(
    const SceneLifecycleStatus &status,
    const ScenePanelTheme &theme
) {
  if (status.preview != ScenePreviewState::Current) {
    return SceneMenuEntryPresentation{
        .status_text = scene_preview_state_label(status.preview),
        .status_color = scene_preview_state_color(theme, status.preview),
    };
  }

  return SceneMenuEntryPresentation{
      .status_text = scene_runtime_state_label(status.runtime),
      .status_color = scene_runtime_state_color(theme, status.runtime),
  };
}

std::string scene_runtime_status_hint(
    const SceneLifecycleStatus &status,
    std::optional<SceneSessionKind> active_session_kind,
    std::optional<SceneExecutionState> active_execution_state
) {
  if (active_execution_state.has_value()) {
    if (active_session_kind == SceneSessionKind::Preview) {
      switch (*active_execution_state) {
        case SceneExecutionState::Playing:
          return "Running preview session";
        case SceneExecutionState::Paused:
          return "Preview session is paused";
        case SceneExecutionState::Stopped:
          return "Preview session is stopped";
        case SceneExecutionState::Static:
          break;
      }
    }

    if (active_session_kind == SceneSessionKind::Runtime) {
      switch (*active_execution_state) {
        case SceneExecutionState::Playing:
          return "Running promoted runtime artifact";
        case SceneExecutionState::Paused:
          return "Runtime session is paused";
        case SceneExecutionState::Stopped:
          return "Runtime session is stopped";
        case SceneExecutionState::Static:
          break;
      }
    }
  }

  if (status.preview == ScenePreviewState::Missing) {
    return "Promote source to preview to create preview";
  }

  if (status.preview == ScenePreviewState::Stale) {
    return "Preview is behind source";
  }

  switch (status.runtime) {
    case SceneRuntimeState::Missing:
      return "Promote preview to create runtime";
    case SceneRuntimeState::BehindPreview:
      return "Runtime is behind preview";
    case SceneRuntimeState::Promoted:
      return {};
    case SceneRuntimeState::Error:
      return "Runtime artifact has an error";
  }

  return {};
}

bool can_promote_from_active_session(
    bool has_active_scene,
    std::optional<SceneSessionKind> active_session_kind
) {
  return has_active_scene && active_session_kind.has_value() &&
         *active_session_kind != SceneSessionKind::Runtime;
}

std::string runtime_prompt_title(ScenePreviewState state) {
  switch (state) {
    case ScenePreviewState::Missing:
      return "Preview is missing";
    case ScenePreviewState::Stale:
      return "Preview is out of date";
    case ScenePreviewState::Current:
      return "Preview is ready";
    case ScenePreviewState::Error:
      return "Preview has an error";
  }

  return "Preview";
}

std::string runtime_prompt_title(
    std::optional<SceneSessionKind> target_kind,
    const SceneLifecycleStatus &status
) {
  if (target_kind == SceneSessionKind::Runtime) {
    switch (status.runtime) {
      case SceneRuntimeState::Missing:
        return "Runtime is missing";
      case SceneRuntimeState::BehindPreview:
        return "Runtime is behind preview";
      case SceneRuntimeState::Promoted:
        return "Runtime is ready";
      case SceneRuntimeState::Error:
        return "Runtime has an error";
    }
  }

  return runtime_prompt_title(status.preview);
}

std::string runtime_prompt_body(
    std::optional<SceneSessionKind> current_kind,
    std::optional<SceneSessionKind> target_kind,
    const SceneLifecycleStatus &status
) {
  if (target_kind == SceneSessionKind::Runtime) {
    switch (status.runtime) {
      case SceneRuntimeState::Missing:
        return current_kind == SceneSessionKind::Preview
                   ? "Promote the current preview session to runtime before entering runtime."
                   : "Enter preview and promote it to runtime before entering runtime.";
      case SceneRuntimeState::BehindPreview:
        return "Promote the current preview session again to refresh runtime.";
      case SceneRuntimeState::Promoted:
        return "Runtime is ready to run.";
      case SceneRuntimeState::Error:
        return "Promote the current preview session to replace the broken runtime.";
    }
  }

  switch (status.preview) {
    case ScenePreviewState::Missing:
      return "Promote the current source session to preview before entering preview.";
    case ScenePreviewState::Stale:
      return "Promote the current source session again to refresh preview.";
    case ScenePreviewState::Current:
      return "Preview is ready to run.";
    case ScenePreviewState::Error:
      return "Promote the current source session to replace the broken preview.";
  }

  return {};
}

std::string format_file_size(uintmax_t bytes) {
  if (bytes < 1024u) {
    return std::to_string(bytes) + " B";
  }

  if (bytes < 1024u * 1024u) {
    return std::to_string(bytes / 1024u) + " KB";
  }

  return std::to_string(bytes / (1024u * 1024u)) + " MB";
}

std::string scene_session_revision_label(const Scene *scene, SceneSessionKind kind) {
  if (scene == nullptr) {
    return {};
  }

  switch (kind) {
    case SceneSessionKind::Source:
      return {};
    case SceneSessionKind::Preview:
      if (const auto &preview_info = scene->get_preview_build_info();
          preview_info.has_value() && preview_info->source_revision > 0u) {
        return "rev " + std::to_string(preview_info->source_revision);
      }
      return {};
    case SceneSessionKind::Runtime:
      if (const auto &runtime_info = scene->get_runtime_promotion_info();
          runtime_info.has_value() &&
          runtime_info->promoted_from_preview_revision > 0u) {
        return "rev " +
               std::to_string(runtime_info->promoted_from_preview_revision);
      }
      return {};
  }

  return {};
}

std::string scene_artifact_activity_label(
    const ProjectSceneEntryConfig &entry,
    SceneSessionKind kind
) {
  auto project = active_project();
  if (project == nullptr) {
    return {};
  }

  const auto &relative_path = artifact_path_for_kind(entry, kind);
  if (relative_path.empty()) {
    return {};
  }

  const auto absolute_path = project->resolve_path(relative_path);
  std::error_code error_code;
  const auto write_time = std::filesystem::last_write_time(
      absolute_path, error_code
  );
  if (error_code) {
    return {};
  }

  const auto age = relative_age_label(
      std::optional<std::filesystem::file_time_type>{write_time}
  );
  if (age.empty()) {
    return {};
  }

  return artifact_activity_prefix(kind) + age;
}

std::vector<ArtifactInfo> gather_artifact_info(const ProjectSceneEntryConfig &entry) {
  auto project = active_project();
  std::vector<ArtifactInfo> artifacts;
  artifacts.reserve(3);

  auto resolve_size = [&](const std::string &relative_path) -> std::string {
    if (project == nullptr || relative_path.empty()) {
      return "";
    }
    auto full_path = project->resolve_path(relative_path);
    std::error_code error_code;
    auto size = std::filesystem::file_size(full_path, error_code);
    if (error_code) {
      return "";
    }
    return format_file_size(size);
  };

  artifacts.push_back(ArtifactInfo{
      .label = "Source",
      .path = entry.source_path,
      .size = resolve_size(entry.source_path),
  });
  artifacts.push_back(ArtifactInfo{
      .label = "Preview",
      .path = entry.preview_path,
      .size = resolve_size(entry.preview_path),
  });
  artifacts.push_back(ArtifactInfo{
      .label = "Runtime",
      .path = entry.runtime_path,
      .size = resolve_size(entry.runtime_path),
  });

  return artifacts;
}

std::vector<DerivedEntityInfo> gather_derived_entity_info(
    const Scene &scene,
    const ScenePanelTheme &theme
) {
  std::vector<DerivedEntityInfo> entries;
  const auto &state = scene.get_derived_state();
  entries.reserve(state.overrides.size() + state.suppressions.size());

  for (const auto &override_record : state.overrides) {
    entries.push_back(DerivedEntityInfo{
        .name = override_record.key.generator_id + " / " +
                override_record.key.stable_key,
        .detail = derived_override_detail(override_record),
        .state_text = "Override",
        .state_color = theme.accent,
    });
  }

  for (const auto &suppression : state.suppressions) {
    entries.push_back(DerivedEntityInfo{
        .name = suppression.key.generator_id + " / " +
                suppression.key.stable_key,
        .detail = "Entity hidden from derived output",
        .state_text = "Suppressed",
        .state_color = danger_color(),
    });
  }

  return entries;
}

std::string derived_entity_summary(const Scene &scene) {
  const auto &state = scene.get_derived_state();
  return std::to_string(state.overrides.size()) + " overrides · " +
         std::to_string(state.suppressions.size()) + " suppressed";
}

SerializationSummary gather_serialization_summary(const Scene &scene) {
  SerializationSummary summary;
  auto project = active_project();
  summary.format = project != nullptr
                       ? serialization_format_label(
                             project->get_config().serialization.format
                         )
                       : "JSON";
  summary.components = std::to_string(
                           inspector_panel::component_descriptor_count()
                       ) +
                       " registered types";

  const auto entity_snapshots =
      serialization::collect_scene_snapshots(scene.world());
  size_t visible_component_total = 0u;
  for (const auto &snapshot : entity_snapshots) {
    visible_component_total +=
        inspector_panel::visible_component_count(snapshot.components);
  }

  summary.snapshots =
      std::to_string(entity_snapshots.size()) + " entities · " +
      std::to_string(visible_component_total) + " components";
  return summary;
}

const char *startup_target_label(SceneStartupTarget target) {
  switch (target) {
    case SceneStartupTarget::Source:
      return "Source";
    case SceneStartupTarget::Preview:
      return "Preview";
    case SceneStartupTarget::Runtime:
      return "Runtime";
  }
  return "Source";
}

std::string status_bar_warning(
    const SceneLifecycleStatus &status,
    const Scene *scene
) {
  if (status.source == SceneSourceSaveState::Dirty) {
    return "Source has unsaved changes.";
  }

  if (status.preview == ScenePreviewState::Stale) {
    if (scene != nullptr && scene->get_session_kind() == SceneSessionKind::Source) {
      if (const auto &preview_info = scene->get_preview_build_info();
          preview_info.has_value() &&
          scene->world().revision() > preview_info->source_revision) {
        const auto gap = scene->world().revision() - preview_info->source_revision;
        return revision_gap_label(gap, "Preview") + " behind source.";
      }
    }

    return "Preview is behind source.";
  }

  if (status.preview == ScenePreviewState::Missing) {
    return "Preview artifact is missing.";
  }

  if (status.runtime == SceneRuntimeState::BehindPreview) {
    if (scene != nullptr) {
      const auto &preview_info = scene->get_preview_build_info();
      const auto &runtime_info = scene->get_runtime_promotion_info();
      if (preview_info.has_value() && runtime_info.has_value() &&
          preview_info->source_revision >
              runtime_info->promoted_from_preview_revision) {
        const auto gap = preview_info->source_revision -
                         runtime_info->promoted_from_preview_revision;
        return revision_gap_label(gap, "Runtime") + " behind preview.";
      }
    }

    return "Runtime is behind preview.";
  }

  if (status.runtime == SceneRuntimeState::Missing &&
      status.preview == ScenePreviewState::Current) {
    return "Runtime artifact is missing.";
  }

  return {};
}

std::string status_bar_hint(const SceneLifecycleStatus &status) {
  if (status.source == SceneSourceSaveState::Dirty) {
    return "Save source to persist changes.";
  }

  if (status.preview == ScenePreviewState::Stale ||
      status.preview == ScenePreviewState::Missing) {
    return "Save source and promote to preview to bring pipeline up to date.";
  }

  if (status.runtime == SceneRuntimeState::BehindPreview ||
      status.runtime == SceneRuntimeState::Missing) {
    return "Promote preview to runtime to bring pipeline up to date.";
  }

  return {};
}

bool status_bar_visible(const SceneLifecycleStatus &status) {
  return status.source == SceneSourceSaveState::Dirty ||
         status.preview == ScenePreviewState::Stale ||
         status.preview == ScenePreviewState::Missing ||
         status.runtime == SceneRuntimeState::BehindPreview ||
         (status.runtime == SceneRuntimeState::Missing &&
          status.preview == ScenePreviewState::Current);
}

} // namespace astralix::editor::scene_panel
