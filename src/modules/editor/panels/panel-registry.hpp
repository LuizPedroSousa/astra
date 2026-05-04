#pragma once

#include "base-manager.hpp"
#include "panel-controller.hpp"

namespace astralix::editor {

class PanelRegistry : public BaseManager<PanelRegistry> {
public:
  bool register_provider(PanelProviderDescriptor descriptor);
  const PanelProviderDescriptor *find(std::string_view id) const;
  std::vector<const PanelProviderDescriptor *> providers() const;
  void clear();

private:
  std::vector<PanelProviderDescriptor> m_providers;
};

inline Ref<PanelRegistry> panel_registry() {
  if (PanelRegistry::get() == nullptr) {
    PanelRegistry::init();
  }

  return PanelRegistry::get();
}

} // namespace astralix::editor
