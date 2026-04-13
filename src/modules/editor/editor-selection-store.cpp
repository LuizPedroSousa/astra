#include "editor-selection-store.hpp"

#include "log.hpp"

namespace astralix::editor {
namespace {

bool same_entity(
    const std::optional<EntityID> &lhs,
    const std::optional<EntityID> &rhs
) {
  if (lhs.has_value() != rhs.has_value()) {
    return false;
  }

  if (!lhs.has_value()) {
    return true;
  }

  return static_cast<uint64_t>(*lhs) == static_cast<uint64_t>(*rhs);
}

} // namespace

void EditorSelectionStore::set_selected_entity(
    std::optional<EntityID> entity_id
) {
  if (same_entity(m_selected_entity_id, entity_id)) {
    return;
  }

  LOG_INFO("[EditorSelectionStore] set_selected_entity: ",
           entity_id.has_value() ? static_cast<int64_t>(static_cast<uint64_t>(*entity_id)) : -1);
  m_selected_entity_id = entity_id;
  ++m_revision;
}

void EditorSelectionStore::clear_selected_entity() {
  set_selected_entity(std::nullopt);
}

} // namespace astralix::editor
