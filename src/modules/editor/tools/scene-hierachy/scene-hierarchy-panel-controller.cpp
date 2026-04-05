#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include "editor-selection-store.hpp"
#include "fnv1a.hpp"
#include "serialization-context-readers.hpp"
#include "tools/scene-hierachy/helpers.hpp"

#include <algorithm>
#include <utility>

namespace astralix::editor {
namespace {

constexpr double k_snapshot_poll_interval_seconds = 0.2;

} // namespace

void SceneHierarchyPanelController::mount(const PanelMountContext &context) {
  m_runtime = context.runtime;
  m_default_font_id = context.default_font_id;
  m_default_font_size = context.default_font_size;
  m_last_selection_revision = editor_selection_store()->revision();
  m_snapshot_poll_elapsed = 0.0;

  if (m_group_open.empty()) {
    scene_hierarchy_panel::seed_default_group_open(m_group_open);
  }

  refresh(true);
}

void SceneHierarchyPanelController::unmount() {
  close_menus();
  m_runtime = nullptr;
  m_add_component_options.clear();
  m_add_component_lookup.clear();
  m_all_entities.clear();
  m_entities.clear();
  m_visible_rows.clear();
  m_create_button_widget = ui::im::k_invalid_widget_id;
  m_create_3d_trigger_widget = ui::im::k_invalid_widget_id;
  m_create_light_trigger_widget = ui::im::k_invalid_widget_id;
  m_row_add_component_trigger_widget = ui::im::k_invalid_widget_id;
  m_rows_widget = ui::im::k_invalid_widget_id;
  m_last_selection_revision = 0u;
  m_last_clicked_entity_id.reset();
  m_last_click_time = 0.0;
  m_elapsed_time = 0.0;
  m_snapshot_poll_elapsed = 0.0;
  m_has_scene = false;
  m_scene_name.clear();
  m_empty_title.clear();
  m_empty_body.clear();
}

void SceneHierarchyPanelController::update(const PanelUpdateContext &context) {
  m_elapsed_time += context.dt;
  m_snapshot_poll_elapsed += context.dt;

  if (editor_selection_store()->revision() != m_last_selection_revision) {
    refresh(true);
    return;
  }

  if (m_snapshot_poll_elapsed >= k_snapshot_poll_interval_seconds) {
    refresh();
  }
}

std::optional<uint64_t> SceneHierarchyPanelController::render_version() const {
  uint64_t hash = k_fnv1a64_offset_basis;
  hash = fnv1a64_append_value(hash, m_render_revision);
  hash = fnv1a64_append_value(hash, m_rows_widget.value);

  if (m_runtime != nullptr && m_rows_widget != ui::im::k_invalid_widget_id) {
    const auto scroll_state = m_runtime->virtual_list_state(m_rows_widget);
    hash = fnv1a64_append_value(hash, scroll_state.scroll_offset.x);
    hash = fnv1a64_append_value(hash, scroll_state.scroll_offset.y);
    hash = fnv1a64_append_value(hash, scroll_state.viewport_width);
    hash = fnv1a64_append_value(hash, scroll_state.viewport_height);
  }

  return hash;
}

void SceneHierarchyPanelController::load_state(Ref<SerializationContext> state) {
  m_search_query =
      serialization::context::read_string(state, "search_query").value_or("");

  m_group_open.clear();
  scene_hierarchy_panel::seed_default_group_open(m_group_open);

  if (state != nullptr &&
      (*state)["group_open"].kind() == SerializationTypeKind::Object) {
    for (ScopeBucket scope : scene_hierarchy_panel::scope_order) {
      const std::string scope_key = scene_hierarchy_panel::scope_group_key(scope);
      if (const auto open =
              serialization::context::read_bool((*state)["group_open"][scope_key]);
          open.has_value()) {
        m_group_open[scope_key] = *open;
      }

      for (TypeBucket type : scene_hierarchy_panel::type_order) {
        const std::string type_key =
            scene_hierarchy_panel::type_group_key(scope, type);
        if (const auto open =
                serialization::context::read_bool((*state)["group_open"][type_key]);
            open.has_value()) {
          m_group_open[type_key] = *open;
        }
      }
    }
  }

  const auto selected_id =
      serialization::context::read_string(state, "selected_entity_id");
  if (!selected_id.has_value() || selected_id->empty()) {
    m_selected_entity_id.reset();
    editor_selection_store()->clear_selected_entity();
    mark_render_dirty();
    return;
  }

  try {
    m_selected_entity_id = EntityID(std::stoull(*selected_id));
  } catch (...) {
    m_selected_entity_id.reset();
  }

  editor_selection_store()->set_selected_entity(m_selected_entity_id);
  m_last_selection_revision = editor_selection_store()->revision();
  mark_render_dirty();
}

void SceneHierarchyPanelController::save_state(Ref<SerializationContext> state) const {
  if (state == nullptr) {
    return;
  }

  (*state)["selected_entity_id"] =
      m_selected_entity_id.has_value()
          ? static_cast<std::string>(*m_selected_entity_id)
          : std::string{};
  (*state)["search_query"] = m_search_query;

  for (ScopeBucket scope : scene_hierarchy_panel::scope_order) {
    const std::string scope_key = scene_hierarchy_panel::scope_group_key(scope);
    (*state)["group_open"][scope_key] = persisted_group_open(scope_key);

    for (TypeBucket type : scene_hierarchy_panel::type_order) {
      const std::string type_key =
          scene_hierarchy_panel::type_group_key(scope, type);
      (*state)["group_open"][type_key] = persisted_group_open(type_key);
    }
  }
}

void SceneHierarchyPanelController::handle_entity_click(EntityID entity_id) {
  constexpr double k_double_click_threshold = 0.4;

  close_menus();

  const bool is_double_click =
      m_last_clicked_entity_id.has_value() &&
      scene_hierarchy_panel::same_entity(*m_last_clicked_entity_id, entity_id) &&
      (m_elapsed_time - m_last_click_time) < k_double_click_threshold;

  m_last_clicked_entity_id = entity_id;
  m_last_click_time = m_elapsed_time;

  select_entity(entity_id);

  if (is_double_click) {
    editor_selection_store()->request_panel_focus("inspector");
  }
}

void SceneHierarchyPanelController::select_entity(EntityID entity_id) {
  m_selected_entity_id = entity_id;
  editor_selection_store()->set_selected_entity(entity_id);
  refresh(true);
}

const SceneHierarchyPanelController::EntityEntry *
SceneHierarchyPanelController::selected_entry() const {
  if (!m_selected_entity_id.has_value()) {
    return nullptr;
  }

  const auto it = std::find_if(
      m_all_entities.begin(),
      m_all_entities.end(),
      [selected_id = *m_selected_entity_id](const EntityEntry &entry) {
        return scene_hierarchy_panel::same_entity(entry.id, selected_id);
      }
  );
  return it != m_all_entities.end() ? &(*it) : nullptr;
}

bool SceneHierarchyPanelController::persisted_group_open(
    std::string_view key
) const {
  const auto it = m_group_open.find(std::string(key));
  if (it != m_group_open.end()) {
    return it->second;
  }

  return scene_hierarchy_panel::default_group_open(key);
}

void SceneHierarchyPanelController::toggle_group(std::string key) {
  close_menus();
  const bool next_open = !persisted_group_open(key);
  m_group_open[std::move(key)] = next_open;
  refresh(true);
}

} // namespace astralix::editor
