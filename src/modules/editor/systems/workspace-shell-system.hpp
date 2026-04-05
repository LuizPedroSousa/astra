#pragma once

#include "components/transform.hpp"
#include "document/document.hpp"
#include "dsl.hpp"
#include "editor-gizmo-store.hpp"
#include "entities/scene.hpp"
#include "events/key-codes.hpp"
#include "guid.hpp"
#include "layouts/layout-node.hpp"
#include "panels/panel-controller.hpp"
#include "systems/system.hpp"
#include "workspaces/workspace-definition.hpp"
#include "workspaces/workspace-store.hpp"
#include <optional>
#include <unordered_map>
#include <utility>

namespace astralix::editor {

struct WorkspaceShellSystemConfig {
  std::optional<input::KeyCode> toggle_visibility_key = input::KeyCode::F7;
};

class WorkspaceShellSystem : public System<WorkspaceShellSystem> {
public:
  explicit WorkspaceShellSystem(
      WorkspaceShellSystemConfig config = {}
  )
      : m_config(std::move(config)) {}

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

private:
  struct GizmoDragState {
    EditorGizmoHandle handle = EditorGizmoHandle::None;
    EntityID entity_id;
    scene::Transform start_transform;
    glm::vec3 pivot = glm::vec3(0.0f);
    glm::vec3 axis = glm::vec3(0.0f);
    glm::vec3 plane_normal = glm::vec3(0.0f);
    glm::vec3 start_hit_point = glm::vec3(0.0f);
    glm::vec3 start_ring_vector = glm::vec3(0.0f);
    float gizmo_scale = 1.0f;
  };

  struct MountedPanel {
    PanelInstanceSpec spec;
    Scope<PanelController> controller;
    ui::UINodeId content_host_node = ui::k_invalid_node_id;
    std::unique_ptr<ui::im::Runtime> runtime;
    std::optional<uint64_t> last_render_version;
  };

  struct SplitRuntimeNodes {
    ui::UINodeId first = ui::k_invalid_node_id;
    ui::UINodeId second = ui::k_invalid_node_id;
  };

  struct PendingTabActivation {
    std::string path;
    std::string tab_id;
  };

  void ensure_scene_root();
  void load_initial_workspace();
  void activate_workspace(std::string workspace_id);
  void rebuild_workspace_document();
  bool active_workspace_uses_floating_panels() const;
  bool root_should_be_visible() const;
  void set_shell_visible(bool visible);
  void suspend_shell_panels();
  void resume_shell_panels();
  void sync_root_visibility();
  void sync_gizmo_capture_state();
  void update_gizmo_interaction();
  void clear_gizmo_drag_state();
  std::optional<EntityID> pick_entity_at_cursor(
      glm::vec2 cursor,
      const ui::UIRect &interaction_rect
  ) const;
  std::optional<ui::UIRect> visible_document_rect(ui::UINodeId node_id) const;
  ui::dsl::NodeSpec build_shell();
  ui::dsl::NodeSpec build_floating_shell();
  ui::dsl::NodeSpec build_layout_node(const LayoutNode &node, const std::string &path);
  ui::dsl::NodeSpec build_leaf_panel(std::string_view panel_instance_id);
  ui::dsl::NodeSpec build_floating_panel(std::string_view panel_instance_id);
  void mount_panels_from_snapshot();
  void destroy_unmounted_panels();
  void reset_mounted_panel_runtime(MountedPanel &mounted);
  void mount_rendered_panel(std::string_view instance_id, MountedPanel &mounted);
  void render_mounted_panel(MountedPanel &mounted);
  void apply_pending_requests();
  void sync_runtime_layout_state();
  void save_active_workspace();
  WorkspaceSnapshot make_snapshot() const;
  WorkspaceSnapshot snapshot_from_definition(
      const WorkspaceDefinition &workspace, const LayoutTemplate &layout
  ) const;
  bool panel_instance_open(std::string_view panel_instance_id) const;
  bool layout_node_visible(const LayoutNode &node) const;
  bool panel_instance_rendered(
      std::string_view panel_instance_id
  ) const;
  PanelMinimumSize panel_minimum_size(
      std::string_view panel_instance_id
  ) const;
  PanelMinimumSize layout_node_minimum_size(const LayoutNode &node) const;
  bool panel_instance_rendered_in_layout(
      const LayoutNode &node, std::string_view panel_instance_id
  ) const;
  std::string resolved_active_tab(const LayoutNode &node) const;
  LayoutNode *find_layout_node(std::string_view path);
  LayoutNode *find_tabs_node(std::string_view path);
  void focus_panel(std::string_view panel_instance_id);
  WorkspacePanelFrame
  resolve_floating_panel_frame(std::string_view panel_instance_id) const;
  void set_panel_open(std::string_view panel_instance_id, bool open);
  std::vector<ui::dsl::NodeSpec> build_floating_panel_buttons();
  void rebuild_panel_toggle_buttons();

  Scene *m_scene = nullptr;
  std::optional<EntityID> m_root_entity_id;
  Ref<ui::UIDocument> m_document = nullptr;
  std::unique_ptr<WorkspaceStore> m_store;
  std::optional<WorkspaceSnapshot> m_active_snapshot;
  std::unordered_map<std::string, MountedPanel> m_panels;
  std::vector<std::string> m_panel_order;
  std::unordered_map<std::string, SplitRuntimeNodes> m_split_runtime_nodes;
  std::unordered_map<std::string, ui::UINodeId> m_floating_panel_nodes;
  std::string m_active_workspace_id;
  WorkspacePresentation m_active_workspace_presentation =
      WorkspacePresentation::Docked;
  ResourceDescriptorID m_default_font_id = "fonts::roboto";
  float m_default_font_size = 18.0f;
  ui::UINodeId m_shell_bar_node = ui::k_invalid_node_id;
  ui::UINodeId m_panel_row_node = ui::k_invalid_node_id;
  std::optional<std::string> m_requested_workspace_id;
  std::optional<std::string> m_pending_panel_focus;
  std::optional<PendingTabActivation> m_pending_tab_activation;
  std::vector<std::pair<std::string, bool>> m_pending_panel_visibility;
  std::optional<GizmoDragState> m_gizmo_drag_state;
  WorkspaceShellSystemConfig m_config;
  bool m_shell_visible = true;
  bool m_needs_rebuild = false;
  bool m_needs_save = false;
  double m_save_accumulator = 0.0;
};

} // namespace astralix::editor
