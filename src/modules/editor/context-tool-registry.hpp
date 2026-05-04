#pragma once

#include "base-manager.hpp"
#include "editor-context-store.hpp"
#include "guid.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace astralix::editor {

struct ContextToolDefinition {
  std::string id;
  std::string tooltip;
  ResourceDescriptorID icon;
  std::optional<std::string> shortcut_label;
  std::string group;
  int order = 0;
};

class ContextToolProvider {
public:
  virtual ~ContextToolProvider() = default;

  virtual EditorContext context() const = 0;
  virtual std::vector<ContextToolDefinition> tools() const = 0;

  virtual void on_tool_activated(const std::string &) {}
  virtual void sync(double) {}
};

struct EditorContextHash {
  size_t operator()(EditorContext context) const noexcept {
    return static_cast<size_t>(context);
  }
};

class ContextToolRegistry : public BaseManager<ContextToolRegistry> {
public:
  bool register_provider(Scope<ContextToolProvider> provider);
  ContextToolProvider *find_provider(EditorContext context) const;
  const std::vector<ContextToolDefinition> &
  tools_for_context(EditorContext context) const;
  void clear();

private:
  std::unordered_map<EditorContext, Scope<ContextToolProvider>, EditorContextHash>
      m_providers;
  std::unordered_map<
      EditorContext,
      std::vector<ContextToolDefinition>,
      EditorContextHash>
      m_cached_tools;
};

inline Ref<ContextToolRegistry> context_tool_registry() {
  if (ContextToolRegistry::get() == nullptr) {
    ContextToolRegistry::init();
  }

  return ContextToolRegistry::get();
}

} // namespace astralix::editor
