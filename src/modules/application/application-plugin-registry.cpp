#include "application-plugin-registry.hpp"

#include "assert.hpp"

namespace astralix {

void ApplicationPluginRegistry::register_plugin(
    std::string id, ApplicationPluginFn fn
) {
  ASTRA_ENSURE(id.empty(), "Plugin id must not be empty");
  ASTRA_ENSURE(!static_cast<bool>(fn), "Plugin callback must be valid");

  if (m_ids.contains(id)) {
    return;
  }

  m_ids.insert(id);
  m_plugins.emplace_back(std::move(id), std::move(fn));
}

void ApplicationPluginRegistry::apply_plugins(
    ApplicationPluginContext &context
) {
  for (auto &[id, plugin] : m_plugins) {
    if (plugin != nullptr) {
      plugin(context);
    }
  }
}

bool ApplicationPluginRegistry::has_plugin(const std::string &id) const {
  return m_ids.contains(id);
}

void ApplicationPluginRegistry::clear() {
  m_plugins.clear();
  m_ids.clear();
}

} // namespace astralix
