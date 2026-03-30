#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include "editor-selection-store.hpp"
#include "tools/scene-hierachy/helpers.hpp"

#include <algorithm>
#include <utility>

namespace astralix::editor {

void SceneHierarchyPanelController::refresh(bool force) {
  if (m_document == nullptr) {
    return;
  }

  const auto shared_selection = editor_selection_store()->selected_entity();
  if (shared_selection.has_value() != m_selected_entity_id.has_value() ||
      (shared_selection.has_value() && m_selected_entity_id.has_value() &&
       !scene_hierarchy_panel::same_entity(
           *shared_selection, *m_selected_entity_id
       ))) {
    m_selected_entity_id = shared_selection;
    force = true;
  }

  const std::optional<EntityID> previous_selection = m_selected_entity_id;
  Snapshot snapshot = collect_snapshot();
  m_all_entities = snapshot.entities;
  std::vector<EntityEntry> next_entities;

  for (const EntityEntry &entry : m_all_entities) {
    if (m_search_query.empty() ||
        scene_hierarchy_panel::contains_case_insensitive(
            entry.name, m_search_query
        ) ||
        scene_hierarchy_panel::contains_case_insensitive(
            entry.kind_label, m_search_query
        ) ||
        scene_hierarchy_panel::contains_case_insensitive(
            entry.meta_label, m_search_query
        )) {
      next_entities.push_back(entry);
    }
  }

  bool list_changed = m_entities.size() != next_entities.size();
  if (!list_changed) {
    for (size_t index = 0u; index < m_entities.size(); ++index) {
      const EntityEntry &current = m_entities[index];
      const EntityEntry &next = next_entities[index];
      if (!scene_hierarchy_panel::same_entity(current.id, next.id) ||
          current.name != next.name || current.kind_label != next.kind_label ||
          current.meta_label != next.meta_label ||
          current.active != next.active ||
          current.scene_backed != next.scene_backed) {
        list_changed = true;
        break;
      }
    }
  }

  if (list_changed) {
    m_entities = std::move(next_entities);
  }

  if (m_selected_entity_id.has_value()) {
    const bool selection_is_valid = std::any_of(
        m_all_entities.begin(),
        m_all_entities.end(),
        [selected_id = *m_selected_entity_id](const EntityEntry &entry) {
          return scene_hierarchy_panel::same_entity(entry.id, selected_id);
        }
    );

    if (!selection_is_valid) {
      m_selected_entity_id.reset();
      editor_selection_store()->clear_selected_entity();
    }
  }

  m_document->set_text(
      m_scene_name_node,
      snapshot.has_scene ? snapshot.scene_name : std::string("No active scene")
  );
  m_document->set_text(
      m_entity_count_node,
      scene_hierarchy_panel::entity_count_label(
          m_entities.size(), m_all_entities.size()
      )
  );
  m_document->set_text(m_search_input_node, m_search_query);

  if (const EntityEntry *entry = selected_entry(); entry != nullptr) {
    m_document->set_text(
        m_selection_text_node,
        "Selected: " + entry->name + " (" +
            static_cast<std::string>(entry->id) + ")"
    );
  } else if (snapshot.has_scene) {
    m_document->set_text(
        m_selection_text_node, std::string("Selected: none")
    );
  } else {
    m_document->set_text(
        m_selection_text_node, std::string("Selection unavailable")
    );
  }

  const bool show_list = snapshot.has_scene && !m_entities.empty();
  m_document->set_visible(m_scroll_node, show_list);
  m_document->set_visible(m_empty_state_node, !show_list);

  if (!snapshot.has_scene) {
    m_document->set_text(m_empty_title_node, "No active scene");
    m_document->set_text(
        m_empty_body_node,
        "SceneManager is not exposing an active scene yet."
    );
  } else if (m_entities.empty()) {
    m_document->set_text(
        m_empty_title_node,
        m_all_entities.empty() ? "World is empty" : "No matching entities"
    );
    m_document->set_text(
        m_empty_body_node,
        m_all_entities.empty()
            ? "Spawn entities into the active scene to populate the hierarchy."
            : "Adjust the search query to show more entities."
    );
  }

  const bool selection_changed =
      previous_selection.has_value() != m_selected_entity_id.has_value() ||
      (previous_selection.has_value() && m_selected_entity_id.has_value() &&
       !scene_hierarchy_panel::same_entity(
           *previous_selection, *m_selected_entity_id
       ));

  sync_virtual_list(force || list_changed || selection_changed);
}

} // namespace astralix::editor
