#pragma once

#include "base-manager.hpp"

#include <cstdint>
#include <string>

namespace astralix::editor {

enum class EditorContext : uint8_t {
  General = 0,
};

class EditorContextStore : public BaseManager<EditorContextStore> {
public:
  EditorContext active_context() const { return m_active_context; }
  void set_active_context(EditorContext context);

  const std::string &active_tool_id() const { return m_active_tool_id; }
  void set_active_tool_id(std::string tool_id);

  uint64_t revision() const { return m_revision; }

private:
  EditorContext m_active_context = EditorContext::General;
  std::string m_active_tool_id = "gizmo.translate";
  uint64_t m_revision = 1u;
};

inline Ref<EditorContextStore> editor_context_store() {
  if (EditorContextStore::get() == nullptr) {
    EditorContextStore::init();
  }

  return EditorContextStore::get();
}

} // namespace astralix::editor
