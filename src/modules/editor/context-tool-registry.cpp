#include "context-tool-registry.hpp"

#include <algorithm>

namespace astralix::editor {

bool ContextToolRegistry::register_provider(Scope<ContextToolProvider> provider) {
  if (provider == nullptr) {
    return false;
  }

  const EditorContext context = provider->context();
  if (m_providers.contains(context)) {
    return false;
  }

  auto tools = provider->tools();
  std::stable_sort(
      tools.begin(),
      tools.end(),
      [](const ContextToolDefinition &lhs, const ContextToolDefinition &rhs) {
        if (lhs.group != rhs.group) {
          return false;
        }

        return lhs.order < rhs.order;
      }
  );

  m_cached_tools.insert_or_assign(context, std::move(tools));
  m_providers.insert_or_assign(context, std::move(provider));
  return true;
}

ContextToolProvider *
ContextToolRegistry::find_provider(EditorContext context) const {
  const auto it = m_providers.find(context);
  return it != m_providers.end() ? it->second.get() : nullptr;
}

const std::vector<ContextToolDefinition> &
ContextToolRegistry::tools_for_context(EditorContext context) const {
  static const std::vector<ContextToolDefinition> k_empty_tools;

  const auto it = m_cached_tools.find(context);
  return it != m_cached_tools.end() ? it->second : k_empty_tools;
}

void ContextToolRegistry::clear() {
  m_cached_tools.clear();
  m_providers.clear();
}

} // namespace astralix::editor
