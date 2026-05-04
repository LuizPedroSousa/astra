#include "systems/workspace-shell-system-internal.hpp"

namespace astralix::editor {
using namespace workspace_shell_detail;
namespace {

constexpr std::string_view k_context_toolbox_panel_id = "context_toolbox";
constexpr std::string_view k_interaction_hud_panel_id = "interaction_hud";
constexpr std::string_view k_navigation_gizmo_panel_id = "navigation_gizmo";
constexpr std::string_view k_viewport_panel_id = "viewport";
constexpr std::string_view k_inspector_panel_id = "inspector";
constexpr std::string_view k_tools_workspace_id = "tools";

bool inject_context_toolbox_before_viewport(LayoutNode &node) {
  switch (node.kind) {
    case LayoutNodeKind::Leaf:
      if (node.panel_instance_id != k_viewport_panel_id) {
        return false;
      }

      node = LayoutNode::split(
          ui::FlexDirection::Row,
          0.08f,
          LayoutNode::leaf(std::string(k_context_toolbox_panel_id)),
          LayoutNode::leaf(std::string(k_viewport_panel_id)),
          LayoutSplitBehavior{
              .resizable = false,
              .show_divider = false,
              .first_extent = ui::UILength::pixels(48.0f),
          }
      );
      return true;

    case LayoutNodeKind::Split:
      if (node.first != nullptr &&
          inject_context_toolbox_before_viewport(*node.first)) {
        return true;
      }

      if (node.second != nullptr &&
          inject_context_toolbox_before_viewport(*node.second)) {
        return true;
      }

      return false;

    case LayoutNodeKind::Tabs:
    default:
      return false;
  }
}

bool layout_is_inspector_leaf(const LayoutNode *node) {
  return node != nullptr && node->kind == LayoutNodeKind::Leaf &&
         node->panel_instance_id == k_inspector_panel_id;
}

bool inject_interaction_hud_into_right_column(LayoutNode &node) {
  switch (node.kind) {
    case LayoutNodeKind::Split:
      if (node.split_axis == ui::FlexDirection::Column &&
          node.first != nullptr && node.second != nullptr &&
          layout_is_inspector_leaf(node.second.get())) {
        LayoutNode preserved = node;
        node = LayoutNode::split(
            ui::FlexDirection::Column,
            0.24f,
            LayoutNode::leaf(std::string(k_interaction_hud_panel_id)),
            std::move(preserved),
            LayoutSplitBehavior{
                .resizable = false,
                .show_divider = false,
                .first_extent = ui::UILength::pixels(184.0f),
            }
        );
        return true;
      }

      if (node.first != nullptr &&
          inject_interaction_hud_into_right_column(*node.first)) {
        return true;
      }

      if (node.second != nullptr &&
          inject_interaction_hud_into_right_column(*node.second)) {
        return true;
      }

      return false;

    case LayoutNodeKind::Leaf:
    case LayoutNodeKind::Tabs:
    default:
      return false;
  }
}

void merge_missing_panels(
    WorkspaceSnapshot &snapshot,
    const WorkspaceSnapshot &defaults
) {
  for (const auto &[instance_id, panel_state] : defaults.panels) {
    if (snapshot.panels.contains(instance_id)) {
      continue;
    }

    snapshot.panels.insert_or_assign(instance_id, panel_state);
  }
}

void migrate_interaction_hud_panel_state(WorkspaceSnapshot &snapshot) {
  if (snapshot.workspace_id != k_tools_workspace_id) {
    return;
  }

  const auto panel_it =
      snapshot.panels.find(std::string(k_interaction_hud_panel_id));
  if (panel_it == snapshot.panels.end()) {
    return;
  }

  panel_it->second.dock_slot = WorkspaceDockSlot{
      .edge = WorkspaceDockEdge::Right,
      .extent = 360.0f,
      .order = 0,
  };
  panel_it->second.floating_frame.reset();
}

void migrate_tools_viewport_overlay_panel_state(WorkspaceSnapshot &snapshot) {
  if (snapshot.workspace_id != k_tools_workspace_id) {
    return;
  }

  const auto reset_floating_overlay_panel = [&snapshot](std::string_view panel_id) {
    const auto panel_it = snapshot.panels.find(std::string(panel_id));
    if (panel_it == snapshot.panels.end()) {
      return;
    }

    panel_it->second.dock_slot.reset();
    panel_it->second.floating_frame.reset();
  };

  reset_floating_overlay_panel(k_navigation_gizmo_panel_id);
  reset_floating_overlay_panel(k_interaction_hud_panel_id);
}

} // namespace

void WorkspaceShellSystem::load_initial_workspace() {
  const auto registered = workspace_registry()->workspaces();
  if (registered.empty()) {
    m_active_snapshot.reset();
    m_active_workspace_id.clear();
    workspace_ui_store()->publish_state(
        std::string{}, std::vector<ToolbarButtonState>{}
    );
    return;
  }

  std::string workspace_id = "studio";
  if (auto stored = m_store->load_active_workspace_id(); stored.has_value()) {
    workspace_id = *stored;
  } else if (workspace_registry()->find(workspace_id) == nullptr) {
    workspace_id = registered.front()->id;
  }

  activate_workspace(std::move(workspace_id));
}

void WorkspaceShellSystem::activate_workspace(std::string workspace_id) {
  const auto *workspace = workspace_registry()->find(workspace_id);
  if (workspace == nullptr) {
    const auto registered = workspace_registry()->workspaces();
    if (registered.empty()) {
      return;
    }

    workspace = registered.front();
    workspace_id = workspace->id;
  }

  const auto *layout = layout_registry()->find(workspace->layout_id);
  if (layout == nullptr) {
    return;
  }

  if (!m_active_workspace_id.empty() && m_active_snapshot.has_value()) {
    save_active_workspace();
  }

  m_active_workspace_id = workspace_id;
  m_active_workspace_presentation = workspace->presentation;
  m_dock_drop_preview_node = ui::k_invalid_node_id;
  m_dock_drag_state.reset();
  m_panel_order.clear();
  m_panel_order.reserve(workspace->panels.size());
  for (const auto &panel : workspace->panels) {
    m_panel_order.push_back(panel.instance_id);
  }

  if (auto stored_snapshot = m_store->load_snapshot(workspace_id);
      stored_snapshot.has_value()) {
    m_active_snapshot = std::move(*stored_snapshot);
    const WorkspaceSnapshot defaults = snapshot_from_definition(*workspace, *layout);

    merge_missing_panels(*m_active_snapshot, defaults);
    if (m_active_snapshot->version < 4) {
      migrate_interaction_hud_panel_state(*m_active_snapshot);
    }
    if (m_active_snapshot->version < 5) {
      migrate_tools_viewport_overlay_panel_state(*m_active_snapshot);
    }
    if (!layout_contains_panel_slot(
            m_active_snapshot->root, k_context_toolbox_panel_id
        )) {
      inject_context_toolbox_before_viewport(m_active_snapshot->root);
    }
    if (!layout_contains_panel_slot(
            m_active_snapshot->root, k_interaction_hud_panel_id
        )) {
      inject_interaction_hud_into_right_column(m_active_snapshot->root);
    }
    m_active_snapshot->version = k_workspace_snapshot_version;

    for (const auto &[instance_id, panel_state] : m_active_snapshot->panels) {
      if (std::find(m_panel_order.begin(), m_panel_order.end(), instance_id) ==
          m_panel_order.end()) {
        m_panel_order.push_back(instance_id);
      }
    }
  } else {
    m_active_snapshot = snapshot_from_definition(*workspace, *layout);
  }

  if (m_active_snapshot.has_value()) {
    m_active_snapshot->workspace_id = workspace_id;
  }

  mount_panels_from_snapshot();
  publish_workspace_ui_state();
  m_store->save_active_workspace_id(workspace_id);
  m_needs_rebuild = true;
  m_needs_save = true;
  m_save_accumulator = 0.0;
}

void WorkspaceShellSystem::save_active_workspace() {
  if (!m_active_snapshot.has_value()) {
    return;
  }

  auto snapshot = make_snapshot();
  m_store->save_active_workspace_id(snapshot.workspace_id);
  m_store->save_snapshot(snapshot);
  m_active_snapshot = snapshot;
  m_needs_save = false;
}

WorkspaceSnapshot WorkspaceShellSystem::make_snapshot() const {
  WorkspaceSnapshot snapshot =
      m_active_snapshot.has_value() ? *m_active_snapshot : WorkspaceSnapshot{};
  snapshot.version = k_workspace_snapshot_version;
  snapshot.workspace_id = m_active_workspace_id;

  for (const auto &[instance_id, mounted] : m_panels) {
    auto state_it = snapshot.panels.find(instance_id);
    if (state_it == snapshot.panels.end()) {
      state_it = snapshot.panels
                     .emplace(
                         instance_id,
                         WorkspacePanelState{
                             .provider_id = mounted.spec.provider_id,
                             .title = mounted.spec.title,
                             .open = mounted.spec.open,
                             .floating_frame =
                                 active_workspace_uses_floating_panels() &&
                                         !mounted.spec.dock_slot.has_value()
                                     ? std::optional<WorkspacePanelResolvedFrame>(
                                           resolve_floating_panel_frame(instance_id)
                                       )
                                     : std::nullopt,
                             .dock_slot = mounted.spec.dock_slot,
                         }
                     )
                     .first;
    }

    state_it->second.provider_id = mounted.spec.provider_id;
    state_it->second.title = mounted.spec.title;
    state_it->second.open = mounted.spec.open;
    if (active_workspace_uses_floating_panels() &&
        !state_it->second.floating_frame.has_value() &&
        !state_it->second.dock_slot.has_value()) {
      state_it->second.floating_frame =
          resolve_floating_panel_frame(instance_id);
    }

    if (mounted.controller != nullptr) {
      auto ctx = m_store->create_context();
      mounted.controller->save_state(ctx);
      state_it->second.state_blob = m_store->encode_panel_state(ctx);
    }
  }

  return snapshot;
}

WorkspaceSnapshot WorkspaceShellSystem::snapshot_from_definition(
    const WorkspaceDefinition &workspace,
    const LayoutTemplate &layout
) const {
  WorkspaceSnapshot snapshot;
  snapshot.version = k_workspace_snapshot_version;
  snapshot.workspace_id = workspace.id;
  snapshot.root = layout.root;

  for (const auto &panel : workspace.panels) {
    WorkspacePanelState state{
        .provider_id = panel.provider_id,
        .title = panel.title,
        .open = panel.open,
        .dock_slot = panel.dock_slot,
    };

    if (panel.seed_state && m_store != nullptr) {
      auto ctx = m_store->create_context();
      panel.seed_state(ctx);
      state.state_blob = m_store->encode_panel_state(ctx);
    }

    snapshot.panels.emplace(panel.instance_id, std::move(state));
  }

  return snapshot;
}

void WorkspaceShellSystem::publish_workspace_ui_state() {
  std::vector<ToolbarButtonState> toolbar_buttons;
  const auto *workspace = workspace_registry()->find(m_active_workspace_id);
  const std::vector<std::string> panel_ids =
      ordered_shell_panel_ids(workspace, m_active_snapshot, m_panel_order);

  if (m_active_snapshot.has_value()) {
    toolbar_buttons.reserve(panel_ids.size());
    for (const auto &instance_id : panel_ids) {
      if (!panel_instance_toggleable(instance_id)) {
        continue;
      }

      const auto it = m_active_snapshot->panels.find(instance_id);
      if (it == m_active_snapshot->panels.end()) {
        continue;
      }

      toolbar_buttons.push_back(ToolbarButtonState{
          .panel_instance_id = instance_id,
          .title = !it->second.title.empty() ? it->second.title : instance_id,
          .open = it->second.open,
      });
    }
  }

  workspace_ui_store()->publish_state(
      m_active_workspace_id, std::move(toolbar_buttons)
  );
}

} // namespace astralix::editor
