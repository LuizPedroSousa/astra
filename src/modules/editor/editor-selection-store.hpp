#pragma once

#include "base-manager.hpp"
#include "guid.hpp"

#include <optional>
#include <string>

namespace astralix::editor {

class EditorSelectionStore : public BaseManager<EditorSelectionStore> {
public:
  std::optional<EntityID> selected_entity() const { return m_selected_entity_id; }

  void set_selected_entity(std::optional<EntityID> entity_id);
  void clear_selected_entity();

  uint64_t revision() const { return m_revision; }

  void request_panel_focus(std::string panel_id) {
    m_pending_panel_focus = std::move(panel_id);
  }

  std::optional<std::string> consume_panel_focus_request() {
    auto request = std::move(m_pending_panel_focus);
    m_pending_panel_focus.reset();
    return request;
  }

private:
  std::optional<EntityID> m_selected_entity_id;
  uint64_t m_revision = 0u;
  std::optional<std::string> m_pending_panel_focus;
};

inline Ref<EditorSelectionStore> editor_selection_store() {
  if (EditorSelectionStore::get() == nullptr) {
    EditorSelectionStore::init();
  }

  return EditorSelectionStore::get();
}

} // namespace astralix::editor
