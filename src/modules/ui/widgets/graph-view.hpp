#pragma once

#include "glm/glm.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace astralix::ui {

using UIGraphId = uint64_t;

enum class UIGraphPortDirection : uint8_t {
  Input,
  Output,
};

enum class UIGraphHitSemantic : uint32_t {
  None = 0u,
  Background = 1u,
  NodeBody = 2u,
  NodeHeader = 3u,
  Port = 4u,
  Edge = 5u,
  Marquee = 6u,
};

enum class UIGraphDragMode : uint8_t {
  None,
  NodeMove,
  Marquee,
  ConnectionDrag,
};

struct UIGraphPort {
  UIGraphId id = 0u;
  UIGraphId node_id = 0u;
  UIGraphPortDirection direction = UIGraphPortDirection::Input;
  std::string label;
  glm::vec4 color = glm::vec4(1.0f);
};

struct UIGraphNode {
  UIGraphId id = 0u;
  std::string title;
  glm::vec2 position = glm::vec2(0.0f);
  glm::vec2 size_hint = glm::vec2(0.0f);
  std::vector<UIGraphId> input_ports;
  std::vector<UIGraphId> output_ports;
  bool collapsed = false;
};

struct UIGraphEdge {
  UIGraphId id = 0u;
  UIGraphId from_port_id = 0u;
  UIGraphId to_port_id = 0u;
  glm::vec4 color = glm::vec4(1.0f);
  float thickness = 2.0f;
};

struct UIGraphSelection {
  std::vector<UIGraphId> node_ids;
  std::vector<UIGraphId> edge_ids;
};

struct UIGraphViewModel {
  std::vector<UIGraphNode> nodes;
  std::vector<UIGraphPort> ports;
  std::vector<UIGraphEdge> edges;
  UIGraphSelection selection;
};

struct GraphViewSpec {
  UIGraphViewModel model;
};

struct UIGraphSemanticTarget {
  UIGraphHitSemantic semantic = UIGraphHitSemantic::None;
  UIGraphId primary_id = 0u;
  UIGraphId secondary_id = 0u;
};

struct UIGraphConnectionPreview {
  UIGraphId from_port_id = 0u;
  std::optional<UIGraphId> hovered_port_id;
  glm::vec2 current_world_position = glm::vec2(0.0f);
};

struct UIGraphDragState {
  UIGraphDragMode mode = UIGraphDragMode::None;
  UIGraphId primary_id = 0u;
  UIGraphId secondary_id = 0u;
  glm::vec2 press_world_position = glm::vec2(0.0f);
  glm::vec2 current_world_position = glm::vec2(0.0f);
  glm::vec2 origin_world_position = glm::vec2(0.0f);
};

struct UIGraphViewState {
  UIGraphViewModel model;
  std::optional<UIGraphSemanticTarget> hovered_target;
  std::optional<UIGraphSemanticTarget> pressed_target;
  UIGraphDragState drag;
  bool marquee_visible = false;
  glm::vec2 marquee_start_world = glm::vec2(0.0f);
  glm::vec2 marquee_current_world = glm::vec2(0.0f);
  std::optional<UIGraphConnectionPreview> connection_preview;
  std::function<void(const UIGraphSelection &)> on_selection_change;
  std::function<void(UIGraphId node_id, glm::vec2 position)> on_node_move;
  std::function<void(UIGraphId from_port_id, std::optional<UIGraphId>)>
      on_connection_drag_end;
};

} // namespace astralix::ui
