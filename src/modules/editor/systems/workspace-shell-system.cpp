#include "systems/workspace-shell-system-internal.hpp"

namespace astralix::editor {
using namespace workspace_shell_detail;

void WorkspaceShellSystem::handle_scene_instance_invalidation() {
  auto scene_manager = SceneManager::get();
  const uint64_t scene_generation =
      scene_manager != nullptr ? scene_manager->scene_instance_generation()
                               : 0u;
  if (scene_generation == m_scene_instance_generation) {
    return;
  }

  m_scene_instance_generation = scene_generation;
  m_scene = nullptr;
  m_root_entity_id.reset();
  m_local_view_state.reset();
  m_hidden_entity_visibility_state.reset();
  m_camera_navigation_drag_state.reset();
  m_modal_transform_state.reset();
  clear_gizmo_drag_state();
  m_pending_viewport_pick_pixel.reset();
  m_undo_stack.clear();
  m_needs_rebuild = true;
  editor_viewport_hud_store()->clear();
  editor_gizmo_store()->clear_interaction();
}

WorkspaceShellSystem::~WorkspaceShellSystem() {
  if (!m_active_snapshot.has_value() || m_store == nullptr) {
    return;
  }
  sync_runtime_layout_state();
  save_active_workspace();
}

void WorkspaceShellSystem::end() {
  auto scene_manager = SceneManager::get();
  Scene *active_scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;

  if (m_active_snapshot.has_value() && m_store != nullptr) {
    sync_runtime_layout_state();
    save_active_workspace();
  }

  if (m_hidden_entity_visibility_state.has_value()) {
    auto hidden_state = std::move(*m_hidden_entity_visibility_state);
    m_hidden_entity_visibility_state.reset();

    if (hidden_state.scene != nullptr && hidden_state.scene == active_scene) {
      for (const auto &[entity_id, was_active] : hidden_state.active_states) {
        if (!hidden_state.scene->world().contains(entity_id)) {
          continue;
        }

        hidden_state.scene->world().set_active(entity_id, was_active);
      }
    }
  }

  exit_local_view();
  editor_viewport_hud_store()->clear();

  for (auto &[instance_id, mounted] : m_panels) {
    if (mounted.controller != nullptr) {
      mounted.controller->unmount();
    }
    reset_mounted_panel_runtime(mounted);
  }
  m_panels.clear();
  m_panel_order.clear();
  m_split_runtime_nodes.clear();
  m_floating_panel_nodes.clear();
  m_dock_drop_preview_node = ui::k_invalid_node_id;
  m_dock_drag_state.reset();

  if (active_scene != nullptr && active_scene == m_scene &&
      m_root_entity_id.has_value() &&
      active_scene->world().contains(*m_root_entity_id)) {
    active_scene->world().destroy(*m_root_entity_id);
    m_root_entity_id.reset();
  }

  m_document = nullptr;
  m_store.reset();
  m_active_snapshot.reset();
  m_active_workspace_id.clear();
  m_hidden_entity_visibility_state.reset();
  m_undo_stack.clear();
  m_camera_navigation_drag_state.reset();
  m_modal_transform_state.reset();
  m_gizmo_drag_state.reset();
  m_last_camera_navigation_preset = EditorCameraNavigationPreset::Free;
  m_needs_rebuild = false;
  m_needs_save = false;
  m_save_accumulator = 0.0;
}

void WorkspaceShellSystem::start() {
  m_store = std::make_unique<WorkspaceStore>(active_project());
  m_last_camera_navigation_preset = editor_camera_navigation_store()->preset();
  if (auto scene_manager = SceneManager::get(); scene_manager != nullptr) {
    m_scene_instance_generation =
        scene_manager->scene_instance_generation();
  }
  ensure_scene_root();
  load_initial_workspace();
  rebuild_workspace_document();
}

void WorkspaceShellSystem::fixed_update(double) {}

void WorkspaceShellSystem::pre_update(double) {
  if (m_config.toggle_visibility_key.has_value() &&
      input::IS_KEY_RELEASED(*m_config.toggle_visibility_key)) {
    set_shell_visible(!m_shell_visible);
  }
}

void WorkspaceShellSystem::update(double dt) {
  ASTRA_PROFILE_N("WorkspaceShellSystem::update");

  if (auto scene_manager = SceneManager::get(); scene_manager != nullptr) {
    scene_manager->flush_pending_active_scene_state();
  }

  handle_scene_instance_invalidation();
  ensure_scene_root();

  if (!m_active_snapshot.has_value()) {
    editor_viewport_hud_store()->clear();
    editor_gizmo_store()->clear_interaction();
    clear_gizmo_drag_state();
    return;
  }

  for (auto &[instance_id, panel] : m_panels) {
    if (panel.controller != nullptr && panel_instance_rendered(instance_id)) {
      const std::string panel_zone_name =
          panel.spec.title + "::update+render";
      ASTRA_PROFILE_DYN(panel_zone_name.c_str(), panel_zone_name.size());
      panel.controller->update(PanelUpdateContext{.dt = dt});
      render_mounted_panel(panel);
    }
  }

  {
    ASTRA_PROFILE_N("WorkspaceShellSystem::apply_pending_requests");
    apply_pending_requests();
  }
  {
    ASTRA_PROFILE_N("WorkspaceShellSystem::sync_dock_drag_state");
    sync_dock_drag_state();
  }
  {
    ASTRA_PROFILE_N("WorkspaceShellSystem::sync_runtime_layout_state");
    sync_runtime_layout_state();
  }

  if (m_needs_rebuild && root_should_be_visible()) {
    ASTRA_PROFILE_N("WorkspaceShellSystem::rebuild_workspace_document");
    rebuild_workspace_document();
  }

  if (m_needs_save) {
    m_save_accumulator += dt;
    if (m_save_accumulator >= 0.2) {
      save_active_workspace();
      m_save_accumulator = 0.0;
    }
  }

  {
    ASTRA_PROFILE_N("WorkspaceShellSystem::gizmo");
    sync_camera_navigation_preset();
    sync_gizmo_capture_state();
    update_viewport_camera_navigation(dt);
    update_gizmo_interaction();
    sync_viewport_hud();
  }
}

} // namespace astralix::editor
