#include "build.hpp"

#include "editor-selection-store.hpp"
#include "entities/serializers/scene-snapshot.hpp"
#include "managers/scene-manager.hpp"

#include <algorithm>
#include <utility>

namespace astralix::editor {
namespace panel = inspector_panel;

void InspectorPanelController::mount(const PanelMountContext &context) {
  m_document = context.document;
  m_default_font_id = context.default_font_id;
  m_default_font_size = context.default_font_size;
  m_last_selection_revision = editor_selection_store()->revision();
  refresh(true);
}

void InspectorPanelController::unmount() {
  m_document = nullptr;
  m_snapshot = {};
  m_add_component_options.clear();
  m_add_component_lookup.clear();
  m_pending_add_component_name.clear();
  m_scalar_drafts.clear();
  m_group_drafts.clear();
  m_component_expansion.clear();
}

void InspectorPanelController::update(const PanelUpdateContext &) { refresh(); }

InspectorPanelController::InspectedEntitySnapshot
InspectorPanelController::collect_snapshot() const {
  InspectedEntitySnapshot snapshot;

  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  if (scene == nullptr) {
    return snapshot;
  }

  snapshot.has_scene = true;
  snapshot.scene_name = scene->get_name();

  const auto selected_entity_id = editor_selection_store()->selected_entity();
  if (!selected_entity_id.has_value() ||
      !scene->world().contains(*selected_entity_id)) {
    return snapshot;
  }

  snapshot.entity_id = *selected_entity_id;
  auto entity = scene->world().entity(*selected_entity_id);
  snapshot.entity_name = std::string(entity.name());
  snapshot.entity_active = entity.active();
  snapshot.components = serialization::collect_entity_component_snapshots(entity);
  return snapshot;
}

void InspectorPanelController::refresh(bool force) {
  if (m_document == nullptr) {
    return;
  }

  auto selection_store = editor_selection_store();
  if (selection_store->revision() != m_last_selection_revision) {
    force = true;
  }

  InspectedEntitySnapshot next_snapshot = collect_snapshot();
  if (selection_store->selected_entity().has_value() &&
      !next_snapshot.entity_id.has_value()) {
    selection_store->clear_selected_entity();
    next_snapshot = collect_snapshot();
    force = true;
  }

  if (!force && panel::snapshots_equal(m_snapshot, next_snapshot)) {
    m_last_selection_revision = selection_store->revision();
    return;
  }

  m_snapshot = std::move(next_snapshot);
  m_last_selection_revision = selection_store->revision();
  sync_static_ui();
  rebuild_component_cards();
}

void InspectorPanelController::sync_static_ui() {
  if (m_document == nullptr) {
    return;
  }

  const InspectorPanelTheme theme;
  m_document->set_text(
      m_scene_name_node,
      m_snapshot.has_scene ? m_snapshot.scene_name : std::string("No active scene")
  );
  m_document->set_text(
      m_component_count_node,
      panel::component_count_label(panel::visible_component_count(m_snapshot.components))
  );

  const bool has_selection = m_snapshot.entity_id.has_value();
  m_document->set_enabled(m_entity_name_input_node, has_selection);
  m_document->set_enabled(m_entity_active_node, has_selection);
  m_document->set_checked(m_entity_active_node, has_selection && m_snapshot.entity_active);

  if (has_selection) {
    m_document->set_text(
        m_selection_title_node,
        m_snapshot.entity_name.empty() ? std::string("Unnamed Entity")
                                       : m_snapshot.entity_name
    );
    m_document->set_text(
        m_entity_id_node,
        "Entity ID " + static_cast<std::string>(*m_snapshot.entity_id)
    );
    m_document->set_text(m_entity_name_input_node, m_snapshot.entity_name);
    m_document->mutate_style(m_selection_title_node, [theme](ui::UIStyle &style) {
      style.text_color = theme.text_primary;
    });
  } else {
    m_document->set_text(
        m_selection_title_node,
        m_snapshot.has_scene ? std::string("Nothing selected")
                             : std::string("Selection unavailable")
    );
    m_document->set_text(m_entity_id_node, "Entity ID --");
    m_document->set_text(m_entity_name_input_node, {});
    m_document->mutate_style(m_selection_title_node, [theme](ui::UIStyle &style) {
      style.text_color = theme.text_muted;
    });
  }

  const bool show_components = has_selection;
  m_document->set_visible(m_component_scroll_node, show_components);
  m_document->set_visible(m_empty_state_node, !show_components);

  if (!m_snapshot.has_scene) {
    m_document->set_text(m_empty_title_node, "No active scene");
    m_document->set_text(
        m_empty_body_node,
        "SceneManager is not exposing an active scene yet."
    );
  } else if (!has_selection) {
    m_document->set_text(m_empty_title_node, "Nothing selected");
    m_document->set_text(
        m_empty_body_node,
        "Select an entity in Scene Hierarchy to inspect and edit it."
    );
  }

  sync_add_component_controls();
}

void InspectorPanelController::sync_add_component_controls() {
  if (m_document == nullptr) {
    return;
  }

  m_add_component_options.clear();
  m_add_component_lookup.clear();
  m_pending_add_component_name.clear();

  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  const bool has_selection = scene != nullptr && m_snapshot.entity_id.has_value() &&
                             scene->world().contains(*m_snapshot.entity_id);

  if (!has_selection) {
    m_document->set_select_options(m_add_component_select_node, {});
    m_document->set_enabled(m_add_component_select_node, false);
    m_document->set_enabled(m_add_component_button_node, false);
    return;
  }

  auto entity = scene->world().entity(*m_snapshot.entity_id);
  const auto *descriptors = panel::component_descriptors();
  for (size_t index = 0u; index < panel::component_descriptor_count(); ++index) {
    const auto &descriptor = descriptors[index];
    if (!descriptor.visible || descriptor.can_add == nullptr) {
      continue;
    }

    if (descriptor.can_add(entity)) {
      const std::string label = panel::humanize_token(descriptor.name);
      m_add_component_lookup.emplace(label, descriptor.name);
      m_add_component_options.push_back(label);
    }
  }

  std::sort(m_add_component_options.begin(), m_add_component_options.end());
  if (!m_add_component_options.empty()) {
    m_pending_add_component_name = m_add_component_options.front();
  }

  m_document->set_select_options(
      m_add_component_select_node, m_add_component_options
  );
  m_document->set_selected_index(m_add_component_select_node, 0u);
  m_document->set_enabled(
      m_add_component_select_node, !m_add_component_options.empty()
  );
  m_document->set_enabled(
      m_add_component_button_node, !m_add_component_options.empty()
  );
}

} // namespace astralix::editor
