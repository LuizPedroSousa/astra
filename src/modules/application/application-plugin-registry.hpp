#pragma once

#include "base-manager.hpp"
#include "base.hpp"
#include "managers/project-manager.hpp"
#include "managers/system-manager.hpp"
#include "project.hpp"
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace astralix {

struct ApplicationPluginContext {
  Ref<Project> project = nullptr;
  Ref<SystemManager> systems = nullptr;
};

using ApplicationPluginFn = std::function<void(ApplicationPluginContext &)>;

class ApplicationPluginRegistry : public BaseManager<ApplicationPluginRegistry> {
public:
  void register_plugin(std::string id, ApplicationPluginFn fn);
  void apply_plugins(ApplicationPluginContext &context);
  bool has_plugin(const std::string &id) const;
  void clear();

private:
  std::vector<std::pair<std::string, ApplicationPluginFn>> m_plugins;
  std::unordered_set<std::string> m_ids;
};

inline Ref<ApplicationPluginRegistry> application_plugin_registry() {
  if (ApplicationPluginRegistry::get() == nullptr) {
    ApplicationPluginRegistry::init();
  }

  return ApplicationPluginRegistry::get();
}

} // namespace astralix
