#include "systems/workspace-shell-system-internal.hpp"

namespace astralix::editor {
using namespace workspace_shell_detail;

bool WorkspaceShellSystem::active_workspace_uses_floating_panels() const {
  return m_active_workspace_presentation ==
         WorkspacePresentation::FloatingPanels;
}

bool WorkspaceShellSystem::root_should_be_visible() const {
  if (active_workspace_uses_floating_panels()) {
    if (!m_active_snapshot.has_value()) {
      return false;
    }

    return std::any_of(
        m_active_snapshot->panels.begin(),
        m_active_snapshot->panels.end(),
        [](const auto &entry) { return entry.second.open; }
    );
  }

  return m_shell_visible;
}

void WorkspaceShellSystem::ensure_scene_root() {
  m_scene =
      SceneManager::get() != nullptr ? SceneManager::get()->get_active_scene()
                                     : nullptr;
  if (m_scene == nullptr) {
    return;
  }

  auto &scene_world = m_scene->world();
  if (auto existing_root_entity_id =
          find_workspace_shell_root_entity(scene_world);
      existing_root_entity_id.has_value()) {
    m_root_entity_id = *existing_root_entity_id;

    if (auto *root = scene_world.get<rendering::UIRoot>(*m_root_entity_id);
        root != nullptr) {
      configure_workspace_ui_root(
          *root, m_document, m_default_font_id, m_default_font_size
      );
    }

    return;
  }

  auto root = m_scene->spawn_entity(std::string(k_workspace_shell_root_name));
  root.emplace<rendering::UIRoot>(rendering::UIRoot{
      .document = m_document,
      .default_font_id = m_default_font_id,
      .default_font_size = m_default_font_size,
      .sort_order = 200,
      .input_enabled = true,
      .visible = true,
  });
  m_root_entity_id = root.id();
}

void WorkspaceShellSystem::rebuild_workspace_document() {
  if (!m_active_snapshot.has_value()) {
    return;
  }

  ensure_scene_root();
  destroy_unmounted_panels();

  for (auto &[instance_id, mounted] : m_panels) {
    if (mounted.controller != nullptr) {
      mounted.controller->unmount();
    }
    reset_mounted_panel_runtime(mounted);
  }

  m_split_runtime_nodes.clear();
  m_floating_panel_nodes.clear();
  m_dock_drop_preview_node = ui::k_invalid_node_id;

  auto document = ui::UIDocument::create();
  document->set_root_font_size(m_default_font_size);
  ui::dsl::mount(*document, build_shell());

  m_document = document;

  if (m_scene != nullptr && m_root_entity_id.has_value()) {
    if (auto *root = m_scene->world().get<rendering::UIRoot>(*m_root_entity_id);
        root != nullptr) {
      configure_workspace_ui_root(
          *root, m_document, m_default_font_id, m_default_font_size
      );
    }
  }

  sync_root_visibility();

  for (auto &[instance_id, mounted] : m_panels) {
    if (mounted.controller != nullptr && panel_instance_rendered(instance_id)) {
      mount_rendered_panel(instance_id, mounted);
    }
  }

  m_needs_rebuild = false;
}

void WorkspaceShellSystem::set_shell_visible(bool visible) {
  if (m_shell_visible == visible) {
    return;
  }

  if (active_workspace_uses_floating_panels()) {
    m_shell_visible = visible;
    if (m_shell_visible && m_needs_rebuild) {
      rebuild_workspace_document();
      return;
    }
    sync_root_visibility();
    return;
  }

  if (visible) {
    m_shell_visible = true;
    resume_shell_panels();
    return;
  }

  suspend_shell_panels();
  m_shell_visible = false;
  sync_root_visibility();
}

void WorkspaceShellSystem::suspend_shell_panels() {
  if (!m_active_snapshot.has_value()) {
    return;
  }

  for (auto &[instance_id, mounted] : m_panels) {
    auto panel_state = m_active_snapshot->panels.find(instance_id);
    if (panel_state != m_active_snapshot->panels.end() &&
        mounted.controller != nullptr) {
      auto ctx = m_store->create_context();
      mounted.controller->save_state(ctx);
      panel_state->second.state_blob = m_store->encode_panel_state(ctx);
    }

    if (mounted.controller != nullptr) {
      mounted.controller->unmount();
    }
    reset_mounted_panel_runtime(mounted);
  }

  m_panels.clear();
  save_active_workspace();
}

void WorkspaceShellSystem::resume_shell_panels() {
  if (!m_active_snapshot.has_value()) {
    sync_root_visibility();
    return;
  }

  mount_panels_from_snapshot();
  rebuild_workspace_document();
  sync_root_visibility();
}

void WorkspaceShellSystem::sync_root_visibility() {
  ensure_scene_root();
  if (m_scene == nullptr || !m_root_entity_id.has_value()) {
    return;
  }

  if (auto *root = m_scene->world().get<rendering::UIRoot>(*m_root_entity_id);
      root != nullptr) {
    const bool visible = root_should_be_visible();
    root->visible = visible;
    root->input_enabled = visible;
    if (visible && root->document != nullptr) {
      root->document->mark_layout_dirty();
    }
  }
}

std::optional<ui::UIRect>
WorkspaceShellSystem::visible_document_rect(ui::UINodeId node_id) const {
  if (m_document == nullptr || node_id == ui::k_invalid_node_id) {
    return std::nullopt;
  }

  const auto *node = m_document->node(node_id);
  if (node == nullptr || !node->visible || node->layout.bounds.width <= 0.0f ||
      node->layout.bounds.height <= 0.0f) {
    return std::nullopt;
  }

  return node->layout.bounds;
}

} // namespace astralix::editor
