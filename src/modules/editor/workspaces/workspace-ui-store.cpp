#include "workspaces/workspace-ui-store.hpp"

namespace astralix::editor {
namespace {

bool toolbar_buttons_equal(
    const std::vector<ToolbarButtonState> &lhs,
    const std::vector<ToolbarButtonState> &rhs
) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t index = 0; index < lhs.size(); ++index) {
    const auto &lhs_button = lhs[index];
    const auto &rhs_button = rhs[index];
    if (lhs_button.panel_instance_id != rhs_button.panel_instance_id ||
        lhs_button.title != rhs_button.title ||
        lhs_button.open != rhs_button.open) {
      return false;
    }
  }

  return true;
}

} // namespace

void WorkspaceUIStore::publish_state(
    std::string active_workspace_id,
    std::vector<ToolbarButtonState> toolbar_buttons
) {
  if (m_active_workspace_id == active_workspace_id &&
      toolbar_buttons_equal(m_toolbar_buttons, toolbar_buttons)) {
    return;
  }

  m_active_workspace_id = std::move(active_workspace_id);
  m_toolbar_buttons = std::move(toolbar_buttons);
  ++m_revision;
}

void WorkspaceUIStore::request_workspace_activation(std::string workspace_id) {
  m_pending_workspace_activation = std::move(workspace_id);
}

std::optional<std::string>
WorkspaceUIStore::consume_workspace_activation_request() {
  auto request = std::move(m_pending_workspace_activation);
  m_pending_workspace_activation.reset();
  return request;
}

void WorkspaceUIStore::request_panel_visibility(
    std::string panel_instance_id,
    bool open
) {
  m_pending_panel_visibility.emplace_back(std::move(panel_instance_id), open);
}

std::vector<std::pair<std::string, bool>>
WorkspaceUIStore::consume_panel_visibility_requests() {
  auto requests = std::move(m_pending_panel_visibility);
  m_pending_panel_visibility.clear();
  return requests;
}

void WorkspaceUIStore::request_scene_hierarchy_create_menu() {
  m_pending_scene_hierarchy_create_menu = true;
}

bool WorkspaceUIStore::consume_scene_hierarchy_create_menu_request() {
  const bool pending = m_pending_scene_hierarchy_create_menu;
  m_pending_scene_hierarchy_create_menu = false;
  return pending;
}

void WorkspaceUIStore::request_inspector_entity_name_focus() {
  m_pending_inspector_entity_name_focus = true;
}

bool WorkspaceUIStore::consume_inspector_entity_name_focus_request() {
  const bool pending = m_pending_inspector_entity_name_focus;
  m_pending_inspector_entity_name_focus = false;
  return pending;
}

} // namespace astralix::editor
