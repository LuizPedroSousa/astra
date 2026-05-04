#pragma once

#include "base.hpp"
#include "serialization-context.hpp"
#include "types.hpp"
#include <cstdint>
#include <functional>
#include <optional>

namespace astralix::editor {

enum class WorkspacePresentation : uint8_t {
  Docked = 0,
  FloatingPanels = 1,
};

enum class WorkspaceFloatingPlacement : uint8_t {
  Absolute = 0,
  TopLeft = 1,
  TopCenter = 2,
  TopRight = 3,
  LeftCenter = 4,
  Center = 5,
  RightCenter = 6,
  BottomLeft = 7,
  BottomCenter = 8,
  BottomRight = 9,
};

enum class WorkspaceDockEdge : uint8_t {
  Left = 0,
  Top = 1,
  Right = 2,
  Bottom = 3,
  Center = 4,
};

struct WorkspaceDockSlot {
  WorkspaceDockEdge edge = WorkspaceDockEdge::Left;
  float extent = 280.0f;
  int order = 0;
};

struct WorkspacePanelFrame {
  ui::UILength x = ui::UILength::pixels(0.0f);
  ui::UILength y = ui::UILength::pixels(0.0f);
  ui::UILength width = ui::UILength::pixels(0.0f);
  ui::UILength height = ui::UILength::pixels(0.0f);

  bool valid() const {
    return width.unit != ui::UILengthUnit::Auto &&
           height.unit != ui::UILengthUnit::Auto;
  }
};

struct WorkspacePanelResolvedFrame {
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
  std::optional<WorkspaceDockSlot> dock_slot;
  WorkspaceFloatingPlacement floating_placement =
      WorkspaceFloatingPlacement::Absolute;
  bool floating_draggable = true;
  bool floating_resizable = true;
  float floating_shell_opacity = 1.0f;
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
