#include "editor-context-store.hpp"

#include "context-tool-registry.hpp"

#include <algorithm>
#include <string_view>

namespace astralix::editor {
namespace {

const ContextToolDefinition *find_tool(
    const std::vector<ContextToolDefinition> &tools,
    std::string_view tool_id
) {
  const auto it = std::find_if(
      tools.begin(),
      tools.end(),
      [tool_id](const ContextToolDefinition &tool) {
        return tool.id == tool_id;
      }
  );
  return it != tools.end() ? &(*it) : nullptr;
}

} // namespace

void EditorContextStore::set_active_context(EditorContext context) {
  if (m_active_context == context) {
    return;
  }

  m_active_context = context;

  const auto &tools = context_tool_registry()->tools_for_context(context);
  if (!tools.empty() && find_tool(tools, m_active_tool_id) == nullptr) {
    m_active_tool_id = tools.front().id;
  }

  if (auto *provider = context_tool_registry()->find_provider(m_active_context);
      provider != nullptr && !m_active_tool_id.empty()) {
    provider->on_tool_activated(m_active_tool_id);
  }

  ++m_revision;
}

void EditorContextStore::set_active_tool_id(std::string tool_id) {
  if (tool_id.empty() || m_active_tool_id == tool_id) {
    return;
  }

  const auto &tools =
      context_tool_registry()->tools_for_context(m_active_context);
  if (!tools.empty() && find_tool(tools, tool_id) == nullptr) {
    return;
  }

  m_active_tool_id = std::move(tool_id);
  if (auto *provider = context_tool_registry()->find_provider(m_active_context);
      provider != nullptr) {
    provider->on_tool_activated(m_active_tool_id);
  }

  ++m_revision;
}

} // namespace astralix::editor
