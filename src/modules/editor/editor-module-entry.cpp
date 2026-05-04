#include "module-api.h"

#include "builtin-plugins.hpp"
#include "commands/builtin-editor-commands.hpp"
#include "editor-camera-navigation-store.hpp"
#include "context-tool-registry.hpp"
#include "editor-context-store.hpp"
#include "editor-gizmo-store.hpp"
#include "editor-selection-store.hpp"
#include "layouts/layout-registry.hpp"
#include "managers/system-manager.hpp"
#include "panels/panel-registry.hpp"
#include "editor-viewport-navigation-store.hpp"
#include "systems/render-system/render-system.hpp"
#include "systems/scene-system.hpp"
#include "systems/ui-system/ui-system.hpp"
#include "systems/workspace-shell-system.hpp"
#include "workspaces/workspace-registry.hpp"
#include "workspaces/workspace-ui-store.hpp"

#include <cassert>

namespace {

using namespace astralix;
using namespace astralix::editor;

void editor_load(void *config, uint32_t config_size) {
  assert(config_size == sizeof(WorkspaceShellSystemConfig));
  auto shell_config =
      *static_cast<const WorkspaceShellSystemConfig *>(config);

  panel_registry();
  layout_registry();
  workspace_registry();
  workspace_ui_store();
  context_tool_registry();
  editor_camera_navigation_store();
  editor_context_store();
  editor_selection_store();
  editor_gizmo_store();
  editor_viewport_navigation_store();

  register_builtin_context_providers();
  register_builtin_panels();
  register_builtin_layouts();
  register_builtin_workspaces();
  register_builtin_editor_commands();

  auto system_manager = SystemManager::get();
  auto *workspace_shell = system_manager->add_system<WorkspaceShellSystem>(shell_config);
  auto *scene_system = system_manager->get_system<SceneSystem>();
  auto *ui_system = system_manager->get_system<UISystem>();
  auto *render_system = system_manager->get_system<RenderSystem>();

  if (workspace_shell != nullptr && ui_system != nullptr) {
    workspace_shell->add_dependencies(ui_system);
  }
  if (ui_system != nullptr && scene_system != nullptr) {
    ui_system->add_dependencies(scene_system);
  }
  if (render_system != nullptr && workspace_shell != nullptr) {
    render_system->add_dependencies(workspace_shell);
  }

  system_manager->update_system_work_order();
  system_manager->start_system<WorkspaceShellSystem>();
}

void editor_unload() {
  SystemManager::get()->remove_system<WorkspaceShellSystem>();
  context_tool_registry()->clear();
  panel_registry()->clear();
  layout_registry()->clear();
  workspace_registry()->clear();
}

} // namespace

extern "C" __attribute__((visibility("default")))
const AstraModuleAPI *astra_get_module_api() {
  static AstraModuleAPI api{
      ASTRA_MODULE_API_VERSION,
      editor_load,
      editor_unload,
  };
  return &api;
}
