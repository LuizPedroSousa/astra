#pragma once

#include "context-tool-registry.hpp"

namespace astralix::editor {

class GizmoContextToolProvider final : public ContextToolProvider {
public:
  EditorContext context() const override { return EditorContext::General; }
  std::vector<ContextToolDefinition> tools() const override;

  void on_tool_activated(const std::string &tool_id) override;
  void sync(double dt) override;
};

} // namespace astralix::editor
