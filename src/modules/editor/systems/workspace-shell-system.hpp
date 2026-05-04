#pragma once

#include "base.hpp"
#include "components/transform.hpp"
#include "document/document.hpp"
#include "dsl.hpp"
#include "editor-camera-navigation-store.hpp"
#include "editor-gizmo-store.hpp"
#include "editor-viewport-hud-store.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "entities/scene.hpp"
#include "events/key-codes.hpp"
#include "guid.hpp"
#include "layouts/layout-node.hpp"
#include "panels/panel-controller.hpp"
#include "systems/system.hpp"
#include "workspaces/workspace-definition.hpp"
#include "workspaces/workspace-store.hpp"
#include "workspaces/workspace-ui-store.hpp"
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astralix::editor {

  struct WorkspaceShellSystemConfig {
    std::optional<input::KeyCode> toggle_visibility_key = input::KeyCode::F7;
  };

  class WorkspaceShellSystem : public System<WorkspaceShellSystem> {
  public:
    explicit WorkspaceShellSystem(
      WorkspaceShellSystemConfig config = {}
    )
      : m_config(std::move(config)) {
    }

    ~WorkspaceShellSystem() override;

    void start() override;
    void end() override;
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

    struct ModalTransformState {
      enum class ConstraintKind : uint8_t {
        None = 0,
        Axis = 1,
        Plane = 2,
      };

      EditorGizmoMode mode = EditorGizmoMode::Translate;
      EntityID entity_id;
      scene::Transform origin_transform;
      scene::Transform start_transform;
      glm::vec3 pivot = glm::vec3(0.0f);
      glm::vec3 axis = glm::vec3(0.0f);
      glm::vec3 plane_normal = glm::vec3(0.0f);
      glm::vec3 start_hit_point = glm::vec3(0.0f);
      glm::vec3 start_ring_vector = glm::vec3(0.0f);
      glm::vec2 start_cursor = glm::vec2(0.0f);
      float gizmo_scale = 1.0f;
      ConstraintKind constraint = ConstraintKind::None;
      std::optional<EditorGizmoHandle> visual_handle;
    };

    struct CameraNavigationDragState {
      glm::vec3 pivot = glm::vec3(0.0f);
    };

    struct LocalViewState {
      Scene *scene = nullptr;
      EntityID focus_entity_id;
      bool focus_entity_was_active = true;
      std::vector<std::pair<EntityID, bool>> renderable_states;
    };

    struct HiddenEntityVisibilityState {
      Scene *scene = nullptr;
      std::unordered_map<EntityID, bool> active_states;
    };

    struct TransformUndoEntry {
      EntityID entity_id;
      scene::Transform before;
    };

    struct RestoreDeletedEntityUndoEntry {
      Scene *scene = nullptr;
      serialization::EntitySnapshot snapshot;
    };

    struct EditorShortcutModifierLatch {
      bool active = false;
      bool shift = false;
      bool control = false;
      bool alt = false;
      bool super = false;
    };

    struct EditorUndoEntry {
      enum class Kind : uint8_t {
        Transform = 0,
        RestoreDeletedEntity = 1,
      };

      Kind kind = Kind::Transform;
      TransformUndoEntry transform;
      RestoreDeletedEntityUndoEntry restore_deleted_entity;
    };

    struct MountedPanel {
      PanelInstanceSpec spec;
      Scope<PanelController> controller;
      ui::UINodeId content_host_node = ui::k_invalid_node_id;
      Scope<ui::im::Runtime> runtime;
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

    struct DockDropZone {
      WorkspaceDockEdge edge = WorkspaceDockEdge::Left;
      ui::UIRect target_rect;
      ui::UIRect preview_rect;
    };

    struct DockDragState {
      std::string panel_instance_id;
      std::optional<DockDropZone> active_drop_zone;
      bool layout_changed = false;
      bool started_docked = false;
    };

    struct DockLayoutMetrics {
      ui::UIRect workspace_bounds;
      ui::UIRect floating_bounds;
      float left_extent = 0.0f;
      float top_extent = 0.0f;
      float right_extent = 0.0f;
      float bottom_extent = 0.0f;
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
    void exit_local_view();
    void toggle_local_view(std::optional<EntityID> focus_entity_id);
    void sync_camera_navigation_preset();
    void update_viewport_camera_navigation(double dt);
    void sync_viewport_hud();
    void sync_gizmo_capture_state();
    void update_gizmo_interaction();
    void push_transform_undo(
      EntityID entity_id,
      const scene::Transform& before,
      const scene::Transform& after
    );
    void push_restore_deleted_entity_undo(
      Scene &scene,
      serialization::EntitySnapshot snapshot
    );
    bool try_undo_last_action();
    void clear_gizmo_drag_state();
    std::optional<EntityID> pick_entity_at_cursor(
      glm::vec2 cursor,
      const ui::UIRect& interaction_rect
    );
    std::optional<ui::UIRect> visible_document_rect(ui::UINodeId node_id) const;
    ui::dsl::NodeSpec build_shell();
    ui::dsl::NodeSpec build_floating_shell();
    ui::dsl::NodeSpec build_dock_drop_preview(
      std::string_view panel_instance_id,
      const std::optional<DockDropZone>& active_zone
    ) const;
    ui::dsl::NodeSpec build_layout_node(const LayoutNode& node, const std::string& path);
    ui::dsl::NodeSpec build_leaf_panel(std::string_view panel_instance_id);
    ui::dsl::NodeSpec build_floating_panel(std::string_view panel_instance_id);
    void mount_panels_from_snapshot();
    void destroy_unmounted_panels();
    void reset_mounted_panel_runtime(MountedPanel& mounted);
    void mount_rendered_panel(std::string_view instance_id, MountedPanel& mounted);
    void render_mounted_panel(MountedPanel& mounted);
    void apply_pending_requests();
    void sync_dock_drag_state();
    void sync_dock_drop_preview();
    void sync_runtime_layout_state();
    void save_active_workspace();
    WorkspaceSnapshot make_snapshot() const;
    WorkspaceSnapshot snapshot_from_definition(
      const WorkspaceDefinition& workspace, const LayoutTemplate& layout
    ) const;
    void publish_workspace_ui_state();
    bool panel_instance_open(std::string_view panel_instance_id) const;
    const PanelProviderDescriptor* panel_provider(
      std::string_view panel_instance_id
    ) const;
    bool panel_instance_toggleable(std::string_view panel_instance_id) const;
    bool panel_instance_has_shell_frame(
      std::string_view panel_instance_id
    ) const;
    bool layout_node_visible(const LayoutNode& node) const;
    bool panel_instance_rendered(
      std::string_view panel_instance_id
    ) const;
    PanelMinimumSize panel_minimum_size(
      std::string_view panel_instance_id
    ) const;
    PanelMinimumSize layout_node_minimum_size(const LayoutNode& node) const;
    bool panel_instance_rendered_in_layout(
      const LayoutNode& node, std::string_view panel_instance_id
    ) const;
    std::string resolved_active_tab(const LayoutNode& node) const;
    LayoutNode* find_layout_node(std::string_view path);
    LayoutNode* find_tabs_node(std::string_view path);
    void focus_panel(std::string_view panel_instance_id);
    std::optional<WorkspaceDockSlot>
    panel_dock_slot(std::string_view panel_instance_id) const;
    std::vector<std::string>
    docked_panel_ids(WorkspaceDockEdge edge) const;
    std::vector<DockDropZone>
    dock_drop_zones(std::string_view panel_instance_id) const;
    DockLayoutMetrics compute_dock_layout_metrics(
      std::string_view exclude_panel_instance_id = {}
    ) const;
    WorkspacePanelResolvedFrame
    resolve_workspace_panel_frame(std::string_view panel_instance_id) const;
    WorkspacePanelResolvedFrame
      resolve_floating_panel_frame(std::string_view panel_instance_id) const;
    std::optional<DockDropZone> detect_dock_drop_zone(
      std::string_view panel_instance_id,
      glm::vec2 cursor
    ) const;
    void dock_panel_to_edge(
      std::string_view panel_instance_id,
      WorkspaceDockEdge edge
    );
    void undock_panel(std::string_view panel_instance_id);
    void set_panel_open(std::string_view panel_instance_id, bool open);
    void handle_scene_instance_invalidation();

    Scene* m_scene = nullptr;
    std::optional<EntityID> m_root_entity_id;
    Ref<ui::UIDocument> m_document = nullptr;
    Scope<WorkspaceStore> m_store;
    std::optional<WorkspaceSnapshot> m_active_snapshot;
    std::unordered_map<std::string, MountedPanel> m_panels;
    std::vector<std::string> m_panel_order;
    std::unordered_map<std::string, SplitRuntimeNodes> m_split_runtime_nodes;
    std::unordered_map<std::string, ui::UINodeId> m_floating_panel_nodes;
    ui::UINodeId m_dock_drop_preview_node = ui::k_invalid_node_id;
    std::optional<DockDragState> m_dock_drag_state;
    std::string m_active_workspace_id;
    WorkspacePresentation m_active_workspace_presentation =
      WorkspacePresentation::Docked;
    ResourceDescriptorID m_default_font_id = "fonts::roboto";
    float m_default_font_size = 18.0f;
    std::optional<glm::ivec2> m_pending_viewport_pick_pixel;
    std::optional<std::string> m_requested_workspace_id;
    std::optional<std::string> m_pending_panel_focus;
    std::optional<PendingTabActivation> m_pending_tab_activation;
    std::vector<std::pair<std::string, bool>> m_pending_panel_visibility;
    std::optional<LocalViewState> m_local_view_state;
    std::optional<HiddenEntityVisibilityState> m_hidden_entity_visibility_state;
    std::vector<EditorUndoEntry> m_undo_stack;
    std::optional<CameraNavigationDragState> m_camera_navigation_drag_state;
    std::optional<ModalTransformState> m_modal_transform_state;
    std::optional<GizmoDragState> m_gizmo_drag_state;
    EditorShortcutModifierLatch m_undo_shortcut_latch;
    EditorShortcutModifierLatch m_duplicate_shortcut_latch;
    EditorShortcutModifierLatch m_visibility_shortcut_latch;
    EditorShortcutModifierLatch m_add_shortcut_latch;
    bool m_kp2_view_shortcut_active = false;
    bool m_kp2_view_shortcut_pan = false;
    bool m_kp4_view_shortcut_active = false;
    bool m_kp4_view_shortcut_pan = false;
    bool m_kp6_view_shortcut_active = false;
    bool m_kp6_view_shortcut_pan = false;
    bool m_kp8_view_shortcut_active = false;
    bool m_kp8_view_shortcut_pan = false;
    bool m_kp1_view_shortcut_active = false;
    bool m_kp1_view_shortcut_inverse = false;
    bool m_kp3_view_shortcut_active = false;
    bool m_kp3_view_shortcut_inverse = false;
    bool m_kp7_view_shortcut_active = false;
    bool m_kp7_view_shortcut_inverse = false;
    EditorCameraNavigationPreset m_last_camera_navigation_preset =
        EditorCameraNavigationPreset::Free;
    WorkspaceShellSystemConfig m_config;
    bool m_shell_visible = true;
    bool m_needs_rebuild = false;
    bool m_needs_save = false;
    double m_save_accumulator = 0.0;
    uint64_t m_scene_instance_generation = 0u;
  };

} // namespace astralix::editor
