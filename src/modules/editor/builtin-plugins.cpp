#include "builtin-plugins.hpp"

#include "application-plugin-registry.hpp"
#include "commands/builtin-editor-commands.hpp"
#include "context-tool-registry.hpp"
#include "contexts/gizmo/gizmo-context-tool-provider.hpp"
#include "editor-context-store.hpp"
#include "editor-gizmo-store.hpp"
#include "editor-selection-store.hpp"
#include "layouts/layout-node.hpp"
#include "layouts/layout-registry.hpp"
#include "managers/resource-manager.hpp"
#include "panels/panel-registry.hpp"
#include "path.hpp"
#include "resources/svg.hpp"
#include "systems/workspace-shell-system.hpp"
#include "tools/build-overlay/build-overlay-panel-controller.hpp"
#include "tools/console/console-panel-controller.hpp"
#include "tools/context-toolbox/context-toolbox-panel-controller.hpp"
#include "tools/file-browser/file-browser-panel-controller.hpp"
#include "tools/inspector/inspector-panel-controller.hpp"
#include "tools/interaction-hud/interaction-hud-panel-controller.hpp"
#include "tools/modes/modes-panel-controller.hpp"
#include "tools/navigation-gizmo/navigation-gizmo-panel-controller.hpp"
#include "tools/runtime/runtime-panel-controller.hpp"
#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"
#include "tools/scene/scene-panel-controller.hpp"
#include "tools/shading/shading-panel-controller.hpp"
#include "tools/toolbar/toolbar-panel-controller.hpp"
#include "tools/viewport/viewport-panel-controller.hpp"
#include "types.hpp"
#include "workspaces/workspace-registry.hpp"
#include "workspaces/workspace-ui-store.hpp"

#include "systems/render-system/render-system.hpp"
#include "systems/scene-system.hpp"
#include "systems/ui-system/ui-system.hpp"

namespace astralix::editor {
namespace {

void register_context_toolbox_resources() {
  Svg::create(
      "icons::gizmo_translate",
      Path::create("icons/gizmo-translate.svg", BaseDirectory::Engine)
  );
  Svg::create(
      "icons::gizmo_rotate",
      Path::create("icons/gizmo-rotate.svg", BaseDirectory::Engine)
  );
  Svg::create(
      "icons::gizmo_scale",
      Path::create("icons/gizmo-scale.svg", BaseDirectory::Engine)
  );

  resource_manager()->load_from_descriptors_by_ids<SvgDescriptor>(
      RendererBackend::None,
      {
          "icons::gizmo_translate",
          "icons::gizmo_rotate",
          "icons::gizmo_scale",
      }
  );
}

void ensure_registries() {
  context_tool_registry();
  editor_context_store();
  panel_registry();
  layout_registry();
  workspace_registry();
  workspace_ui_store();
  editor_selection_store();
  editor_gizmo_store();
}

} // namespace

void register_builtin_context_providers() {
  ensure_registries();
  register_context_toolbox_resources();

  context_tool_registry()->register_provider(
      create_scope<GizmoContextToolProvider>()
  );

  if (auto *provider = context_tool_registry()->find_provider(
          editor_context_store()->active_context()
      );
      provider != nullptr) {
    provider->on_tool_activated(editor_context_store()->active_tool_id());
  }
}

void register_builtin_panels() {
  ensure_registries();

  panel_registry()->register_provider({
      .id = "editor.context-toolbox",
      .title = "Context Toolbox",
      .minimum_size = ContextToolboxPanelController::kMinimumSize,
      .singleton = true,
      .show_shell_frame = false,
      .toggleable = false,
      .factory = [] { return create_scope<ContextToolboxPanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.modes",
      .title = "Modes",
      .minimum_size = ModesPanelController::kMinimumSize,
      .singleton = true,
      .show_shell_frame = false,
      .toggleable = false,
      .factory = [] { return create_scope<ModesPanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.toolbar",
      .title = "Toolbar",
      .minimum_size = ToolbarPanelController::kMinimumSize,
      .singleton = true,
      .show_shell_frame = false,
      .toggleable = false,
      .factory = [] { return create_scope<ToolbarPanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.navigation-gizmo",
      .title = "Navigation",
      .minimum_size = NavigationGizmoPanelController::kMinimumSize,
      .singleton = true,
      .show_shell_frame = false,
      .toggleable = false,
      .factory = [] { return create_scope<NavigationGizmoPanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.viewport",
      .title = "Viewport",
      .minimum_size = ViewportPanelController::kMinimumSize,
      .singleton = true,
      .factory = [] { return create_scope<ViewportPanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.interaction-hud",
      .title = "Interaction",
      .minimum_size = InteractionHudPanelController::kMinimumSize,
      .singleton = true,
      .show_shell_frame = false,
      .toggleable = false,
      .factory = [] { return create_scope<InteractionHudPanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.console",
      .title = "Console",
      .minimum_size = ConsolePanelController::kMinimumSize,
      .singleton = true,
      .factory = [] { return create_scope<ConsolePanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.scene",
      .title = "Scene",
      .minimum_size = ScenePanelController::kMinimumSize,
      .singleton = true,
      .factory = [] { return create_scope<ScenePanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.runtime",
      .title = "Runtime",
      .minimum_size = RuntimePanelController::kMinimumSize,
      .singleton = true,
      .factory = [] { return create_scope<RuntimePanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.shading",
      .title = "Shading",
      .minimum_size = ShadingPanelController::kMinimumSize,
      .singleton = true,
      .factory = [] { return create_scope<ShadingPanelController>(); },
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

  panel_registry()->register_provider({
      .id = "editor.file-browser",
      .title = "File Browser",
      .minimum_size = FileBrowserPanelController::kMinimumSize,
      .singleton = true,
      .factory = [] { return create_scope<FileBrowserPanelController>(); },
  });

  panel_registry()->register_provider({
      .id = "editor.build-overlay",
      .title = "Build Overlay",
      .minimum_size = BuildOverlayPanelController::kMinimumSize,
      .singleton = true,
      .show_shell_frame = false,
      .toggleable = false,
      .factory = [] { return create_scope<BuildOverlayPanelController>(); },
  });
}

void register_builtin_layouts() {
  ensure_registries();

  layout_registry()->register_template({
      .id = "studio",
      .title = "Studio",
      .root = LayoutNode::split(
          ui::FlexDirection::Column,
          0.065f,
          LayoutNode::split(
              ui::FlexDirection::Row,
              0.84f,
              LayoutNode::leaf("toolbar"),
              LayoutNode::leaf("modes"),
              LayoutSplitBehavior{
                  .resizable = false,
                  .show_divider = false,
              }
          ),
          LayoutNode::split(
              ui::FlexDirection::Column,
              0.84f,
              LayoutNode::split(
                  ui::FlexDirection::Row,
                  0.18f,
                  LayoutNode::leaf("scene_hierarchy"),
                  LayoutNode::split(
                      ui::FlexDirection::Row,
                      0.08f,
                      LayoutNode::leaf("context_toolbox"),
                      LayoutNode::split(
                          ui::FlexDirection::Row,
                          0.74f,
                          LayoutNode::leaf("viewport"),
                          LayoutNode::split(
                              ui::FlexDirection::Column,
                              0.24f,
                              LayoutNode::leaf("interaction_hud"),
                              LayoutNode::split(
                                  ui::FlexDirection::Column,
                                  0.52f,
                                  LayoutNode::tabs_node(
                                      {"scene", "shading", "runtime", "file_browser"},
                                      "scene"
                                  ),
                                  LayoutNode::leaf("inspector")
                              ),
                              LayoutSplitBehavior{
                                  .resizable = false,
                                  .show_divider = false,
                                  .first_extent = ui::UILength::pixels(184.0f),
                              }
                          )
                      ),
                      LayoutSplitBehavior{
                          .resizable = false,
                          .show_divider = false,
                          .first_extent = ui::UILength::pixels(48.0f),
                      }
                  )
              ),
              LayoutNode::leaf("console")
          ),
          LayoutSplitBehavior{
              .resizable = false,
              .show_divider = false,
              .first_extent = ui::UILength::pixels(56.0f),
          }
      ),
  });

  layout_registry()->register_template({
      .id = "tools",
      .title = "Tools",
      .root = LayoutNode::split(
          ui::FlexDirection::Row,
          0.90f,
          LayoutNode::split(
              ui::FlexDirection::Row,
              0.12f,
              LayoutNode::leaf("toolbar"),
              LayoutNode::split(
                  ui::FlexDirection::Column,
                  0.40f,
                  LayoutNode::leaf("runtime"),
                  LayoutNode::leaf("console")
              )
          ),
          LayoutNode::split(
              ui::FlexDirection::Column,
              0.5f,
              LayoutNode::leaf("modes"),
              LayoutNode::split(
                  ui::FlexDirection::Column,
                  0.5f,
                  LayoutNode::leaf("navigation_gizmo"),
                  LayoutNode::leaf("interaction_hud")
              )
          )
      ),
  });
}

void register_builtin_workspaces() {
  ensure_registries();

  workspace_registry()->register_workspace(
      {
          .id = "studio",
          .title = "Studio",
          .layout_id = "studio",
          .panels = {
              PanelInstanceSpec{
                  .instance_id = "modes",
                  .provider_id = "editor.modes",
                  .title = "Modes",
              },
              PanelInstanceSpec{
                  .instance_id = "toolbar",
                  .provider_id = "editor.toolbar",
                  .title = "Toolbar",
              },
              PanelInstanceSpec{
                  .instance_id = "context_toolbox",
                  .provider_id = "editor.context-toolbox",
                  .title = "Context Toolbox",
              },
              PanelInstanceSpec{
                  .instance_id = "scene_hierarchy",
                  .provider_id = "editor.scene-hierarchy",
                  .title = "Scene Hierarchy",
              },
              PanelInstanceSpec{
                  .instance_id = "file_browser",
                  .provider_id = "editor.file-browser",
                  .title = "File Browser",
              },
              PanelInstanceSpec{
                  .instance_id = "viewport",
                  .provider_id = "editor.viewport",
                  .title = "Viewport",
              },
              PanelInstanceSpec{
                  .instance_id = "interaction_hud",
                  .provider_id = "editor.interaction-hud",
                  .title = "Interaction",
              },
              PanelInstanceSpec{
                  .instance_id = "scene",
                  .provider_id = "editor.scene",
                  .title = "Scene",
              },
              PanelInstanceSpec{
                  .instance_id = "runtime",
                  .provider_id = "editor.runtime",
                  .title = "Runtime",
              },
              PanelInstanceSpec{
                  .instance_id = "shading",
                  .provider_id = "editor.shading",
                  .title = "Shading",
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
              PanelInstanceSpec{
                  .instance_id = "build_overlay",
                  .provider_id = "editor.build-overlay",
                  .title = "Build Overlay",
              },
          },
      }
  );

  workspace_registry()->register_workspace(
      {
          .id = "tools",
          .title = "Tools",
          .layout_id = "tools",
          .presentation = WorkspacePresentation::FloatingPanels,
          .panels = {
              PanelInstanceSpec{
                  .instance_id = "modes",
                  .provider_id = "editor.modes",
                  .title = "Modes",
                  .dock_slot =
                      WorkspaceDockSlot{
                          .edge = WorkspaceDockEdge::Top,
                          .extent = 56.0f,
                          .order = 1,
                      },
                  .floating_draggable = false,
                  .floating_resizable = false,
                  .floating_shell_opacity = 0.0f,
              },
              PanelInstanceSpec{
                  .instance_id = "context_toolbox",
                  .provider_id = "editor.context-toolbox",
                  .title = "Context Toolbox",
                  .dock_slot =
                      WorkspaceDockSlot{
                          .edge = WorkspaceDockEdge::Left,
                          .extent = 48.0f,
                          .order = 0,
                      },
                  .floating_draggable = false,
                  .floating_resizable = false,
                  .floating_shell_opacity = 0.0f,
              },
              PanelInstanceSpec{
                  .instance_id = "toolbar",
                  .provider_id = "editor.toolbar",
                  .title = "Toolbar",
                  .dock_slot =
                      WorkspaceDockSlot{
                          .edge = WorkspaceDockEdge::Top,
                          .extent = 56.0f,
                          .order = 0,
                      },
                  .floating_draggable = false,
                  .floating_resizable = false,
                  .floating_shell_opacity = 0.0f,
              },
              PanelInstanceSpec{
                  .instance_id = "scene_hierarchy",
                  .provider_id = "editor.scene-hierarchy",
                  .title = "Scene Hierarchy",
                  .floating_frame =
                      WorkspacePanelFrame{
                          .x = ui::UILength::pixels(220.0f),
                          .y = ui::UILength::pixels(112.0f),
                          .width = ui::UILength::pixels(360.0f),
                          .height = ui::UILength::pixels(520.0f),
                      },
              },
              PanelInstanceSpec{
                  .instance_id = "file_browser",
                  .provider_id = "editor.file-browser",
                  .title = "File Browser",
                  .floating_frame =
                      WorkspacePanelFrame{
                          .x = ui::UILength::pixels(208.0f),
                          .y = ui::UILength::pixels(540.0f),
                          .width = ui::UILength::pixels(520.0f),
                          .height = ui::UILength::pixels(420.0f),
                      },
              },
              PanelInstanceSpec{
                  .instance_id = "viewport",
                  .provider_id = "editor.viewport",
                  .title = "Viewport",
                  .floating_frame =
                      WorkspacePanelFrame{
                          .x = ui::UILength::pixels(592.0f),
                          .y = ui::UILength::pixels(112.0f),
                          .width = ui::UILength::pixels(1120.0f),
                          .height = ui::UILength::pixels(640.0f),
                      },
              },
              PanelInstanceSpec{
                  .instance_id = "navigation_gizmo",
                  .provider_id = "editor.navigation-gizmo",
                  .title = "Navigation",
                  .floating_frame =
                      WorkspacePanelFrame{
                          .width = ui::UILength::pixels(164.0f),
                          .height = ui::UILength::pixels(186.0f),
                      },
                  .floating_placement = WorkspaceFloatingPlacement::TopRight,
                  .floating_draggable = false,
                  .floating_resizable = false,
                  .floating_shell_opacity = 0.0f,
              },
              PanelInstanceSpec{
                  .instance_id = "interaction_hud",
                  .provider_id = "editor.interaction-hud",
                  .title = "Interaction",
                  .floating_frame =
                      WorkspacePanelFrame{
                          .y = ui::UILength::pixels(198.0f),
                          .width = ui::UILength::pixels(280.0f),
                          .height = ui::UILength::pixels(156.0f),
                      },
                  .floating_placement = WorkspaceFloatingPlacement::TopRight,
                  .floating_draggable = false,
                  .floating_resizable = false,
                  .floating_shell_opacity = 0.0f,
              },
              PanelInstanceSpec{
                  .instance_id = "scene",
                  .provider_id = "editor.scene",
                  .title = "Scene",
                  .floating_frame =
                      WorkspacePanelFrame{
                          .x = ui::UILength::pixels(48.0f),
                          .y = ui::UILength::pixels(112.0f),
                          .width = ui::UILength::pixels(460.0f),
                          .height = ui::UILength::pixels(360.0f),
                      },
              },
              PanelInstanceSpec{
                  .instance_id = "runtime",
                  .provider_id = "editor.runtime",
                  .title = "Runtime",
                  .floating_frame =
                      WorkspacePanelFrame{
                          .x = ui::UILength::pixels(528.0f),
                          .y = ui::UILength::pixels(112.0f),
                          .width = ui::UILength::pixels(560.0f),
                          .height = ui::UILength::pixels(280.0f),
                      },
              },
              PanelInstanceSpec{
                  .instance_id = "shading",
                  .provider_id = "editor.shading",
                  .title = "Shading",
                  .floating_frame =
                      WorkspacePanelFrame{
                          .x = ui::UILength::pixels(1098.0f),
                          .y = ui::UILength::pixels(112.0f),
                          .width = ui::UILength::pixels(560.0f),
                          .height = ui::UILength::pixels(360.0f),
                      },
              },
              PanelInstanceSpec{
                  .instance_id = "inspector",
                  .provider_id = "editor.inspector",
                  .title = "Inspector",
                  .floating_frame =
                      WorkspacePanelFrame{
                          .x = ui::UILength::pixels(640.0f),
                          .y = ui::UILength::pixels(112.0f),
                          .width = ui::UILength::pixels(400.0f),
                          .height = ui::UILength::pixels(520.0f),
                      },
              },
              PanelInstanceSpec{
                  .instance_id = "console",
                  .provider_id = "editor.console",
                  .title = "Console",
                  .floating_frame =
                      WorkspacePanelFrame{
                          .x = ui::UILength::pixels(208.0f),
                          .y = ui::UILength::pixels(196.0f),
                          .width = ui::UILength::pixels(960.0f),
                          .height = ui::UILength::pixels(320.0f),
                      },
              },
              PanelInstanceSpec{
                  .instance_id = "build_overlay",
                  .provider_id = "editor.build-overlay",
                  .title = "Build Overlay",
                  .floating_frame =
                      WorkspacePanelFrame{
                          .x = ui::UILength::pixels(0.0f),
                          .y = ui::UILength::pixels(0.0f),
                          .width = ui::UILength::percent(1.0f),
                          .height = ui::UILength::pixels(600.0f),
                      },
                  .floating_placement = WorkspaceFloatingPlacement::BottomLeft,
                  .floating_draggable = false,
                  .floating_resizable = false,
                  .floating_shell_opacity = 0.0f,
              },
          },
      }
  );
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
  register_builtin_context_providers();
  register_builtin_panels();
  register_builtin_layouts();
  register_builtin_workspaces();
  register_builtin_editor_commands_plugin();
  register_workspace_shell_plugin(config.workspace_shell);
}

} // namespace astralix::editor
