#include "contexts/gizmo/gizmo-context-tool-provider.hpp"

#include "editor-context-store.hpp"
#include "editor-gizmo-store.hpp"

namespace astralix::editor {

std::vector<ContextToolDefinition> GizmoContextToolProvider::tools() const {
  return {
      ContextToolDefinition{
          .id = "gizmo.translate",
          .tooltip = "Translate",
          .icon = "icons::gizmo_translate",
          .shortcut_label = std::string("W"),
          .group = "transform",
          .order = 0,
      },
      ContextToolDefinition{
          .id = "gizmo.rotate",
          .tooltip = "Rotate",
          .icon = "icons::gizmo_rotate",
          .shortcut_label = std::string("E"),
          .group = "transform",
          .order = 1,
      },
      ContextToolDefinition{
          .id = "gizmo.scale",
          .tooltip = "Scale",
          .icon = "icons::gizmo_scale",
          .shortcut_label = std::string("T"),
          .group = "transform",
          .order = 2,
      },
  };
}

void GizmoContextToolProvider::on_tool_activated(const std::string &tool_id) {
  if (tool_id == "gizmo.translate") {
    editor_gizmo_store()->set_mode(EditorGizmoMode::Translate);
    return;
  }

  if (tool_id == "gizmo.rotate") {
    editor_gizmo_store()->set_mode(EditorGizmoMode::Rotate);
    return;
  }

  if (tool_id == "gizmo.scale") {
    editor_gizmo_store()->set_mode(EditorGizmoMode::Scale);
  }
}

void GizmoContextToolProvider::sync(double) {
  std::string expected_tool_id = "gizmo.translate";

  switch (editor_gizmo_store()->mode()) {
    case EditorGizmoMode::Rotate:
      expected_tool_id = "gizmo.rotate";
      break;
    case EditorGizmoMode::Scale:
      expected_tool_id = "gizmo.scale";
      break;
    case EditorGizmoMode::Translate:
    default:
      expected_tool_id = "gizmo.translate";
      break;
  }

  if (editor_context_store()->active_context() != context()) {
    return;
  }

  if (editor_context_store()->active_tool_id() != expected_tool_id) {
    editor_context_store()->set_active_tool_id(std::move(expected_tool_id));
  }
}

} // namespace astralix::editor
