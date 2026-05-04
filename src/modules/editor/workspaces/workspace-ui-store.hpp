#pragma once

#include "base-manager.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace astralix::editor {

struct ToolbarButtonState {
  std::string panel_instance_id;
  std::string title;
  bool open = true;
};

class WorkspaceUIStore : public BaseManager<WorkspaceUIStore> {
public:
  const std::string &active_workspace_id() const { return m_active_workspace_id; }
  const std::vector<ToolbarButtonState> &toolbar_buttons() const {
    return m_toolbar_buttons;
  }
  uint64_t revision() const { return m_revision; }

  void publish_state(
      std::string active_workspace_id,
      std::vector<ToolbarButtonState> toolbar_buttons
  );

  void request_workspace_activation(std::string workspace_id);
  std::optional<std::string> consume_workspace_activation_request();

  void request_panel_visibility(std::string panel_instance_id, bool open);
  std::vector<std::pair<std::string, bool>>
  consume_panel_visibility_requests();

  void request_scene_hierarchy_create_menu();
  bool consume_scene_hierarchy_create_menu_request();

  void request_inspector_entity_name_focus();
  bool consume_inspector_entity_name_focus_request();

private:
  std::string m_active_workspace_id;
  std::vector<ToolbarButtonState> m_toolbar_buttons;
  std::optional<std::string> m_pending_workspace_activation;
  std::vector<std::pair<std::string, bool>> m_pending_panel_visibility;
  bool m_pending_scene_hierarchy_create_menu = false;
  bool m_pending_inspector_entity_name_focus = false;
  uint64_t m_revision = 0u;
};

inline Ref<WorkspaceUIStore> workspace_ui_store() {
  if (WorkspaceUIStore::get() == nullptr) {
    WorkspaceUIStore::init();
  }

  return WorkspaceUIStore::get();
}

} // namespace astralix::editor
