#include "builtin-plugins.hpp"

#include "application-plugin-registry.hpp"
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

namespace astralix::editor {
namespace {

void ensure_registries() {
  panel_registry();
  layout_registry();
  workspace_registry();
  editor_selection_store();
}

} // namespace

void register_builtin_panels() {
  ensure_registries();

  panel_registry()->register_provider({
      .id = "editor.viewport",
      .title = "Viewport",
      .singleton = true,
      .factory = [] { return create_scope<ViewportPanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.console",
      .title = "Console",
      .singleton = true,
      .factory = [] { return create_scope<ConsolePanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.runtime",
      .title = "Runtime",
      .singleton = true,
      .factory = [] { return create_scope<RuntimePanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.inspector",
      .title = "Inspector",
      .singleton = true,
      .factory = [] { return create_scope<InspectorPanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.scene-hierarchy",
      .title = "Scene Hierarchy",
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
          context.systems->add_system<WorkspaceShellSystem>(config);
        }
      }
  );
}

void register_builtin_plugins(BuiltinEditorPluginsConfig config) {
  register_builtin_panels();
  register_builtin_layouts();
  register_builtin_workspaces();
  register_workspace_shell_plugin(config.workspace_shell);
}

} // namespace astralix::editor
