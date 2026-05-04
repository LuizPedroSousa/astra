#pragma once

#include "systems/workspace-shell-system.hpp"

namespace astralix::editor {

struct BuiltinEditorPluginsConfig {
  WorkspaceShellSystemConfig workspace_shell;
};

void register_builtin_plugins(
    BuiltinEditorPluginsConfig config = {}
);
void register_builtin_context_providers();
void register_builtin_panels();
void register_builtin_layouts();
void register_builtin_workspaces();
void register_workspace_shell_plugin(
    WorkspaceShellSystemConfig config = {}
);

} // namespace astralix::editor
