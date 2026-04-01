#include "builtin-plugins.hpp"

#include "application-plugin-registry.hpp"
#include "commands/builtin-editor-commands.hpp"
#include "editor-gizmo-store.hpp"
#include "editor-selection-store.hpp"
#include "layouts/layout-registry.hpp"
#include "panels/panel-registry.hpp"
#include "systems/workspace-shell-system.hpp"
#include "tools/console/console-panel-controller.hpp"
#include "tools/inspector/inspector-panel-controller.hpp"
#include "tools/runtime/runtime-panel-controller.hpp"
#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"
#include "tools/viewport/viewport-panel-controller.hpp"
#include "workspaces/workspace-registry.hpp"

#include "systems/render-system/render-system.hpp"
#include "systems/scene-system.hpp"
#include "systems/ui-system/ui-system.hpp"

namespace astralix::editor {
namespace {

void ensure_registries() {
  panel_registry();
  layout_registry();
  workspace_registry();
  editor_selection_store();
  editor_gizmo_store();
}

} // namespace

void register_builtin_panels() {
  ensure_registries();

  panel_registry()->register_provider({
      .id = "editor.viewport",
      .title = "Viewport",
      .minimum_size = ViewportPanelController::kMinimumSize,
      .singleton = true,
      .factory = [] { return create_scope<ViewportPanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.console",
      .title = "Console",
      .minimum_size = ConsolePanelController::kMinimumSize,
      .singleton = true,
      .factory = [] { return create_scope<ConsolePanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.runtime",
      .title = "Runtime",
      .minimum_size = RuntimePanelController::kMinimumSize,
      .singleton = true,
      .factory = [] { return create_scope<RuntimePanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.inspector",
      .title = "Inspector",
      .minimum_size = InspectorPanelController::kMinimumSize,
      .singleton = true,
      .factory = [] { return create_scope<InspectorPanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.scene-hierarchy",
      .title = "Scene Hierarchy",
      .minimum_size = SceneHierarchyPanelController::kMinimumSize,
      .singleton = true,
      .factory = [] { return create_scope<SceneHierarchyPanelController>(); },
  });
}

void register_builtin_layouts() {
  ensure_registries();

  layout_registry()->register_template({
      .id = "studio.v1",
      .title = "Studio",
      .root = LayoutNode::split(
          ui::FlexDirection::Row,
          0.22f,
          LayoutNode::leaf("scene_hierarchy"),
          LayoutNode::split(
              ui::FlexDirection::Row,
              0.72f,
              LayoutNode::leaf("viewport"),
              LayoutNode::split(
                  ui::FlexDirection::Column,
                  0.44f,
                  LayoutNode::leaf("inspector"),
                  LayoutNode::split(
                      ui::FlexDirection::Column,
                      0.42f,
                      LayoutNode::leaf("runtime"),
                      LayoutNode::leaf("console")
                  )
              )
          )
      ),
  });

  layout_registry()->register_template({
      .id = "tools.v1",
      .title = "Tools",
      .root = LayoutNode::split(
          ui::FlexDirection::Column,
          0.40f,
          LayoutNode::leaf("runtime"),
          LayoutNode::leaf("console")
      ),
  });
}

void register_builtin_workspaces() {
  ensure_registries();

  workspace_registry()->register_workspace({
      .id = "studio",
      .title = "Studio",
      .layout_id = "studio.v1",
      .panels = {
          PanelInstanceSpec{
              .instance_id = "scene_hierarchy",
              .provider_id = "editor.scene-hierarchy",
              .title = "Scene Hierarchy",
          },
          PanelInstanceSpec{
              .instance_id = "viewport",
              .provider_id = "editor.viewport",
              .title = "Viewport",
          },
          PanelInstanceSpec{
              .instance_id = "runtime",
              .provider_id = "editor.runtime",
              .title = "Runtime",
          },
          PanelInstanceSpec{
              .instance_id = "inspector",
              .provider_id = "editor.inspector",
              .title = "Inspector",
          },
          PanelInstanceSpec{
              .instance_id = "console",
              .provider_id = "editor.console",
              .title = "Console",
          },
      },
  });

  workspace_registry()->register_workspace({
      .id = "tools",
      .title = "Tools",
      .layout_id = "tools.v1",
      .presentation = WorkspacePresentation::FloatingPanels,
      .panels = {
          PanelInstanceSpec{
              .instance_id = "scene_hierarchy",
              .provider_id = "editor.scene-hierarchy",
              .title = "Scene Hierarchy",
              .floating_frame =
                  WorkspacePanelFrame{
                      .x = 48.0f,
                      .y = 112.0f,
                      .width = 360.0f,
                      .height = 520.0f,
                  },
          },
          PanelInstanceSpec{
              .instance_id = "viewport",
              .provider_id = "editor.viewport",
              .title = "Viewport",
              .floating_frame =
                  WorkspacePanelFrame{
                      .x = 208.0f,
                      .y = 112.0f,
                      .width = 1120.0f,
                      .height = 640.0f,
                  },
          },
          PanelInstanceSpec{
              .instance_id = "runtime",
              .provider_id = "editor.runtime",
              .title = "Runtime",
              .floating_frame =
                  WorkspacePanelFrame{
                      .x = 48.0f,
                      .y = 112.0f,
                      .width = 560.0f,
                      .height = 280.0f,
                  },
          },
          PanelInstanceSpec{
              .instance_id = "inspector",
              .provider_id = "editor.inspector",
              .title = "Inspector",
              .floating_frame =
                  WorkspacePanelFrame{
                      .x = 640.0f,
                      .y = 112.0f,
                      .width = 400.0f,
                      .height = 520.0f,
                  },
          },
          PanelInstanceSpec{
              .instance_id = "console",
              .provider_id = "editor.console",
              .title = "Console",
              .floating_frame =
                  WorkspacePanelFrame{
                      .x = 208.0f,
                      .y = 196.0f,
                      .width = 960.0f,
                      .height = 320.0f,
                  },
          },
      },
  });
}

void register_workspace_shell_plugin(WorkspaceShellSystemConfig config) {
  application_plugin_registry()->register_plugin(
      "editor.workspace-shell",
      [config](ApplicationPluginContext &context) {
        if (context.systems != nullptr) {
          auto *workspace_shell =
              context.systems->add_system<WorkspaceShellSystem>(config);
          auto *scene_system = context.systems->get_system<SceneSystem>();
          auto *ui_system = context.systems->get_system<UISystem>();
          auto *render_system = context.systems->get_system<RenderSystem>();

          if (workspace_shell != nullptr && ui_system != nullptr) {
            workspace_shell->add_dependencies(ui_system);
          }
          if (ui_system != nullptr && scene_system != nullptr) {
            ui_system->add_dependencies(scene_system);
          }
          if (render_system != nullptr && workspace_shell != nullptr) {
            render_system->add_dependencies(workspace_shell);
          }

          context.systems->update_system_work_order();
        }
      }
  );
}

void register_builtin_editor_commands_plugin() {
  application_plugin_registry()->register_plugin(
      "editor.builtin-commands",
      [](ApplicationPluginContext &) { register_builtin_editor_commands(); }
  );
}

void register_builtin_plugins(BuiltinEditorPluginsConfig config) {
  register_builtin_panels();
  register_builtin_layouts();
  register_builtin_workspaces();
  register_builtin_editor_commands_plugin();
  register_workspace_shell_plugin(config.workspace_shell);
}

} // namespace astralix::editor
