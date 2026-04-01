#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include "editor-selection-store.hpp"
#include "serialization-context-readers.hpp"
#include "tools/scene-hierachy/helpers.hpp"

#include <algorithm>

namespace astralix::editor {

void SceneHierarchyPanelController::mount(const PanelMountContext &context) {
  m_document = context.document;
  m_default_font_id = context.default_font_id;
  m_default_font_size = context.default_font_size;
  m_row_slots.clear();
  m_virtual_list.reset();

  if (m_document != nullptr && m_search_input_node != ui::k_invalid_node_id) {
    m_document->set_text(m_search_input_node, m_search_query);
    m_document->mutate_style(m_search_input_node, [this](ui::UIStyle &style) {
      style.font_id = m_default_font_id;
      style.font_size = std::max(13.0f, m_default_font_size * 0.78f);
    });
  }

  if (m_document != nullptr && m_scroll_node != ui::k_invalid_node_id) {
    m_virtual_list = std::make_unique<ui::VirtualListController>(
        m_document,
        m_scroll_node,
        [this](size_t slot_index) { return create_row_slot(slot_index); },
        [this](size_t slot_index, ui::UINodeId, size_t item_index) {
          bind_row_slot(slot_index, item_index);
        },
        "scene_hierarchy_rows"
    );
    m_virtual_list->set_overscan(3u);
  }

  refresh(true);
}

void SceneHierarchyPanelController::unmount() {
  m_context_entity_id.reset();
  m_add_component_options.clear();
  m_add_component_lookup.clear();
  m_virtual_list.reset();
  m_row_slots.clear();
  m_all_entities.clear();
  m_entities.clear();
  m_document = nullptr;
}

void SceneHierarchyPanelController::update(const PanelUpdateContext &) {
  refresh();
}

void SceneHierarchyPanelController::load_state(Ref<SerializationContext> state) {
  m_search_query =
      serialization::context::read_string(state, "search_query").value_or("");

  const auto selected_id =
      serialization::context::read_string(state, "selected_entity_id");
  if (!selected_id.has_value() || selected_id->empty()) {
    m_selected_entity_id.reset();
    editor_selection_store()->clear_selected_entity();
    return;
  }

  try {
    m_selected_entity_id = EntityID(std::stoull(*selected_id));
  } catch (...) {
    m_selected_entity_id.reset();
  }

  editor_selection_store()->set_selected_entity(m_selected_entity_id);
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

  auto it = std::find_if(
      m_all_entities.begin(),
      m_all_entities.end(),
      [selected_id = *m_selected_entity_id](const EntityEntry &entry) {
        return scene_hierarchy_panel::same_entity(entry.id, selected_id);
      }
  );
  return it != m_all_entities.end() ? &(*it) : nullptr;
}

} // namespace astralix::editor
