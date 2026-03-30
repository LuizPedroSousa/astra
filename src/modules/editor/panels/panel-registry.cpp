#include "panels/panel-registry.hpp"

namespace astralix::editor {

bool PanelRegistry::register_provider(PanelProviderDescriptor descriptor) {
  if (descriptor.id.empty() || !descriptor.factory) {
    return false;
  }

  if (find(descriptor.id) != nullptr) {
    return false;
  }

  m_providers.push_back(std::move(descriptor));
  return true;
}

const PanelProviderDescriptor *PanelRegistry::find(std::string_view id) const {
  for (const auto &descriptor : m_providers) {
    if (descriptor.id == id) {
      return &descriptor;
    }
  }

  return nullptr;
}

std::vector<const PanelProviderDescriptor *> PanelRegistry::providers() const {
  std::vector<const PanelProviderDescriptor *> out;
  out.reserve(m_providers.size());

  for (const auto &descriptor : m_providers) {
    out.push_back(&descriptor);
  }

  return out;
}

} // namespace astralix::editor
