#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include "editor-selection-store.hpp"
#include "tools/scene-hierachy/helpers.hpp"

#include <algorithm>
#include <utility>

namespace astralix::editor {

std::unordered_set<std::string>
SceneHierarchyPanelController::transient_open_groups(
    const std::vector<EntityEntry> &filtered_entities,
    std::optional<EntityID> reveal_entity_id
) const {
  std::unordered_set<std::string> keys;

  if (!m_search_query.empty()) {
    for (const EntityEntry &entry : filtered_entities) {
      keys.insert(scene_hierarchy_panel::scope_group_key(entry.scope_bucket));
      keys.insert(
          scene_hierarchy_panel::type_group_key(
              entry.scope_bucket,
              entry.type_bucket
          )
      );
    }
  }

  if (reveal_entity_id.has_value()) {
    const auto it = std::find_if(
        filtered_entities.begin(),
        filtered_entities.end(),
        [reveal_entity_id](const EntityEntry &candidate) {
          return scene_hierarchy_panel::same_entity(
              candidate.id, *reveal_entity_id
          );
        }
    );
    if (it != filtered_entities.end()) {
      keys.insert(scene_hierarchy_panel::scope_group_key(it->scope_bucket));
      keys.insert(
          scene_hierarchy_panel::type_group_key(
              it->scope_bucket,
              it->type_bucket
          )
      );
    }
  }

  return keys;
}

std::vector<SceneHierarchyPanelController::VisibleRow>
SceneHierarchyPanelController::build_visible_rows(
    const std::vector<EntityEntry> &filtered_entities,
    std::optional<EntityID> reveal_entity_id
) const {
  std::vector<VisibleRow> rows;
  const bool filtered = !m_search_query.empty();
  const auto transient_groups =
      transient_open_groups(filtered_entities, reveal_entity_id);

  for (ScopeBucket scope : scene_hierarchy_panel::scope_order) {
    const size_t scope_total = static_cast<size_t>(std::count_if(
        m_all_entities.begin(),
        m_all_entities.end(),
        [scope](const EntityEntry &entry) {
          return entry.scope_bucket == scope;
        }
    ));
    const size_t scope_visible = static_cast<size_t>(std::count_if(
        filtered_entities.begin(),
        filtered_entities.end(),
        [scope](const EntityEntry &entry) {
          return entry.scope_bucket == scope;
        }
    ));

    if (scope_visible == 0u) {
      continue;
    }

    const std::string scope_key = scene_hierarchy_panel::scope_group_key(scope);
    const bool scope_open =
        persisted_group_open(scope_key) || transient_groups.contains(scope_key);

    rows.push_back(VisibleRow{
        .kind = VisibleRow::Kind::ScopeHeader,
        .group_key = scope_key,
        .title = scene_hierarchy_panel::scope_bucket_label(scope),
        .count_label =
            filtered
                ? scene_hierarchy_panel::entity_count_label(scope_visible, scope_total)
                : scene_hierarchy_panel::entity_count_label(scope_total),
        .scope_bucket = scope,
        .open = scope_open,
        .height = 34.0f,
    });

    if (!scope_open) {
      continue;
    }

    for (TypeBucket type : scene_hierarchy_panel::type_order) {
      const size_t type_total = static_cast<size_t>(std::count_if(
          m_all_entities.begin(),
          m_all_entities.end(),
          [scope, type](const EntityEntry &entry) {
            return entry.scope_bucket == scope && entry.type_bucket == type;
          }
      ));
      const size_t type_visible = static_cast<size_t>(std::count_if(
          filtered_entities.begin(),
          filtered_entities.end(),
          [scope, type](const EntityEntry &entry) {
            return entry.scope_bucket == scope && entry.type_bucket == type;
          }
      ));

      if (type_visible == 0u) {
        continue;
      }

      const std::string type_key =
          scene_hierarchy_panel::type_group_key(scope, type);
      const bool type_open =
          persisted_group_open(type_key) || transient_groups.contains(type_key);

      rows.push_back(VisibleRow{
          .kind = VisibleRow::Kind::TypeHeader,
          .group_key = type_key,
          .title = scene_hierarchy_panel::type_bucket_label(type),
          .count_label =
              filtered
                  ? scene_hierarchy_panel::entity_count_label(type_visible, type_total)
                  : scene_hierarchy_panel::entity_count_label(type_total),
          .scope_bucket = scope,
          .type_bucket = type,
          .open = type_open,
          .height = 32.0f,
      });

      if (!type_open) {
        continue;
      }

      for (const EntityEntry &entry : filtered_entities) {
        if (entry.scope_bucket != scope || entry.type_bucket != type) {
          continue;
        }

        const bool selected =
            m_selected_entity_id.has_value() &&
            scene_hierarchy_panel::same_entity(*m_selected_entity_id, entry.id);
        rows.push_back(VisibleRow{
            .kind = VisibleRow::Kind::Entity,
            .title = entry.name,
            .id_label = "#" + static_cast<std::string>(entry.id),
            .scope_label = entry.scope_label,
            .kind_label = entry.kind_label,
            .entity_id = entry.id,
            .scope_bucket = entry.scope_bucket,
            .type_bucket = entry.type_bucket,
            .active = entry.active,
            .selected = selected,
            .height = selected ? 58.0f : 36.0f,
        });
      }
    }
  }

  return rows;
}

void SceneHierarchyPanelController::refresh(bool force) {
  m_snapshot_poll_elapsed = 0.0;
  const bool previous_has_scene = m_has_scene;
  const std::string previous_scene_name = m_scene_name;
  const std::string previous_empty_title = m_empty_title;
  const std::string previous_empty_body = m_empty_body;

  const auto same_entity_entry = [](const EntityEntry &lhs, const EntityEntry &rhs) {
    return scene_hierarchy_panel::same_entity(lhs.id, rhs.id) &&
           lhs.name == rhs.name &&
           lhs.kind_label == rhs.kind_label &&
           lhs.scope_label == rhs.scope_label &&
           lhs.search_blob == rhs.search_blob &&
           lhs.active == rhs.active &&
           lhs.scene_backed == rhs.scene_backed &&
           lhs.scope_bucket == rhs.scope_bucket &&
           lhs.type_bucket == rhs.type_bucket;
  };
  const auto same_visible_row = [](const VisibleRow &lhs, const VisibleRow &rhs) {
    return lhs.kind == rhs.kind &&
           lhs.group_key == rhs.group_key &&
           lhs.title == rhs.title &&
           lhs.id_label == rhs.id_label &&
           lhs.scope_label == rhs.scope_label &&
           lhs.kind_label == rhs.kind_label &&
           lhs.count_label == rhs.count_label &&
           scene_hierarchy_panel::same_entity(lhs.entity_id, rhs.entity_id) &&
           lhs.scope_bucket == rhs.scope_bucket &&
           lhs.type_bucket == rhs.type_bucket &&
           lhs.open == rhs.open &&
           lhs.active == rhs.active &&
           lhs.selected == rhs.selected &&
           lhs.height == rhs.height;
  };

  auto selection_store = editor_selection_store();
  const uint64_t selection_revision = selection_store->revision();
  const bool selection_revision_changed =
      selection_revision != m_last_selection_revision;
  const auto shared_selection = selection_store->selected_entity();
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

  bool all_entities_changed = m_all_entities.size() != snapshot.entities.size();
  if (!all_entities_changed) {
    all_entities_changed = !std::equal(
        m_all_entities.begin(),
        m_all_entities.end(),
        snapshot.entities.begin(),
        snapshot.entities.end(),
        same_entity_entry
    );
  }

  m_has_scene = snapshot.has_scene;
  m_scene_name = snapshot.scene_name;
  m_all_entities = snapshot.entities;

  std::vector<EntityEntry> next_entities;
  next_entities.reserve(m_all_entities.size());
  for (const EntityEntry &entry : m_all_entities) {
    if (m_search_query.empty() ||
        scene_hierarchy_panel::contains_case_insensitive(
            entry.search_blob, m_search_query
        )) {
      next_entities.push_back(entry);
    }
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

  const std::optional<EntityID> reveal_entity_id =
      selection_revision_changed ? shared_selection : std::nullopt;
  const std::vector<VisibleRow> next_visible_rows =
      build_visible_rows(next_entities, reveal_entity_id);

  bool entities_changed = m_entities.size() != next_entities.size();
  if (!entities_changed) {
    entities_changed = !std::equal(
        m_entities.begin(),
        m_entities.end(),
        next_entities.begin(),
        next_entities.end(),
        same_entity_entry
    );
  }

  bool rows_changed = m_visible_rows.size() != next_visible_rows.size();
  if (!rows_changed) {
    rows_changed = !std::equal(
        m_visible_rows.begin(),
        m_visible_rows.end(),
        next_visible_rows.begin(),
        next_visible_rows.end(),
        same_visible_row
    );
  }

  if (entities_changed || force) {
    m_entities = std::move(next_entities);
  }
  if (rows_changed || force) {
    m_visible_rows = next_visible_rows;
  }

  if (!snapshot.has_scene) {
    m_empty_title = "No active scene";
    m_empty_body = "SceneManager is not exposing an active scene yet.";
  } else if (m_entities.empty()) {
    m_empty_title =
        m_all_entities.empty() ? "World is empty" : "No matching entities";
    m_empty_body =
        m_all_entities.empty()
            ? "Spawn entities into the active scene to populate the hierarchy."
            : "Adjust the search query to show more entities.";
  } else {
    m_empty_title.clear();
    m_empty_body.clear();
  }

  const bool context_is_valid =
      !m_context_entity_id.has_value() ||
      std::any_of(
          m_all_entities.begin(),
          m_all_entities.end(),
          [context_id = *m_context_entity_id](const EntityEntry &entry) {
            return scene_hierarchy_panel::same_entity(entry.id, context_id);
          }
      );
  if (!snapshot.has_scene || !context_is_valid) {
    close_menus();
  }

  const bool selection_changed =
      previous_selection.has_value() != m_selected_entity_id.has_value() ||
      (previous_selection.has_value() && m_selected_entity_id.has_value() &&
       !scene_hierarchy_panel::same_entity(
           *previous_selection, *m_selected_entity_id
       ));
  if (selection_changed && !m_context_entity_id.has_value()) {
    rebuild_add_component_menu();
  }

  m_last_selection_revision = selection_store->revision();

  const bool scene_changed =
      previous_has_scene != m_has_scene || previous_scene_name != m_scene_name;
  const bool empty_state_changed =
      previous_empty_title != m_empty_title || previous_empty_body != m_empty_body;
  if (force || scene_changed || empty_state_changed || all_entities_changed ||
      entities_changed || rows_changed || selection_changed) {
    mark_render_dirty();
  }
}

} // namespace astralix::editor
