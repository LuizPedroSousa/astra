#pragma once

#include "base.hpp"
#include "serialization-context.hpp"
#include <cstdint>
#include <functional>
#include <optional>

namespace astralix::editor {

enum class WorkspacePresentation : uint8_t {
  Docked = 0,
  FloatingPanels = 1,
};

struct WorkspacePanelFrame {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;

  bool valid() const { return width > 1.0f && height > 1.0f; }
};

struct PanelInstanceSpec {
  std::string instance_id;
  std::string provider_id;
  std::string title;
  bool open = true;
  std::optional<WorkspacePanelFrame> floating_frame;
  std::function<void(Ref<SerializationContext>)> seed_state;
};

struct WorkspaceDefinition {
  std::string id;
  std::string title;
  std::string layout_id;
  WorkspacePresentation presentation = WorkspacePresentation::Docked;
  std::vector<PanelInstanceSpec> panels;
};

} // namespace astralix::editor
