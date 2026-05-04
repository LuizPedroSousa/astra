#pragma once

#include "base-manager.hpp"
#include "workspace-definition.hpp"

namespace astralix::editor {

class WorkspaceRegistry : public BaseManager<WorkspaceRegistry> {
public:
  bool register_workspace(WorkspaceDefinition workspace);
  const WorkspaceDefinition *find(std::string_view id) const;
  std::vector<const WorkspaceDefinition *> workspaces() const;
  void clear();

private:
  std::vector<WorkspaceDefinition> m_workspaces;
};

inline Ref<WorkspaceRegistry> workspace_registry() {
  if (WorkspaceRegistry::get() == nullptr) {
    WorkspaceRegistry::init();
  }

  return WorkspaceRegistry::get();
}

} // namespace astralix::editor
