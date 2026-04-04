#include "tools/scene/helpers.hpp"

#include "managers/project-manager.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace astralix::editor::scene_panel {

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
      return "Behind Preview";
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
  return state == ScenePreviewState::Current ? theme.success : theme.accent;
}

glm::vec4 scene_runtime_state_color(
    const ScenePanelTheme &theme,
    SceneRuntimeState state
) {
  return state == SceneRuntimeState::Promoted ? theme.success : theme.accent;
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

} // namespace astralix::editor::scene_panel
