#include "workspaces/workspace-registry.hpp"

namespace astralix::editor {

bool WorkspaceRegistry::register_workspace(WorkspaceDefinition workspace) {
  if (workspace.id.empty() || workspace.layout_id.empty()) {
    return false;
  }

  if (find(workspace.id) != nullptr) {
    return false;
  }

  m_workspaces.push_back(std::move(workspace));
  return true;
}

const WorkspaceDefinition *WorkspaceRegistry::find(std::string_view id) const {
  for (const auto &workspace : m_workspaces) {
    if (workspace.id == id) {
      return &workspace;
    }
  }

  return nullptr;
}

std::vector<const WorkspaceDefinition *> WorkspaceRegistry::workspaces() const {
  std::vector<const WorkspaceDefinition *> out;
  out.reserve(m_workspaces.size());

  for (const auto &workspace : m_workspaces) {
    out.push_back(&workspace);
  }

  return out;
}

void WorkspaceRegistry::clear() { m_workspaces.clear(); }

} // namespace astralix::editor
