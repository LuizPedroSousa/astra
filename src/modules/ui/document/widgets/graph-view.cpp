#include "document/document.hpp"

#include "canvas/view-transform.hpp"
#include "layout/widgets/graph/graph-view.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace astralix::ui {
namespace {

constexpr float k_click_distance_threshold = 2.0f;

bool vec2_equal(const glm::vec2 &lhs, const glm::vec2 &rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool vec4_equal(const glm::vec4 &lhs, const glm::vec4 &rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

bool graph_port_equal(const UIGraphPort &lhs, const UIGraphPort &rhs) {
  return lhs.id == rhs.id && lhs.node_id == rhs.node_id &&
         lhs.direction == rhs.direction && lhs.label == rhs.label &&
         vec4_equal(lhs.color, rhs.color);
}

bool graph_node_equal(const UIGraphNode &lhs, const UIGraphNode &rhs) {
  return lhs.id == rhs.id && lhs.title == rhs.title &&
         vec2_equal(lhs.position, rhs.position) &&
         vec2_equal(lhs.size_hint, rhs.size_hint) &&
         lhs.input_ports == rhs.input_ports &&
         lhs.output_ports == rhs.output_ports &&
         lhs.collapsed == rhs.collapsed;
}

bool graph_edge_equal(const UIGraphEdge &lhs, const UIGraphEdge &rhs) {
  return lhs.id == rhs.id && lhs.from_port_id == rhs.from_port_id &&
         lhs.to_port_id == rhs.to_port_id && vec4_equal(lhs.color, rhs.color) &&
         lhs.thickness == rhs.thickness;
}

bool graph_selection_equal(
    const UIGraphSelection &lhs,
    const UIGraphSelection &rhs
) {
  return lhs.node_ids == rhs.node_ids && lhs.edge_ids == rhs.edge_ids;
}

bool graph_model_equal(
    const UIGraphViewModel &lhs,
    const UIGraphViewModel &rhs
) {
  if (lhs.nodes.size() != rhs.nodes.size() || lhs.ports.size() != rhs.ports.size() ||
      lhs.edges.size() != rhs.edges.size() ||
      !graph_selection_equal(lhs.selection, rhs.selection)) {
    return false;
  }

  for (size_t index = 0u; index < lhs.nodes.size(); ++index) {
    if (!graph_node_equal(lhs.nodes[index], rhs.nodes[index])) {
      return false;
    }
  }

  for (size_t index = 0u; index < lhs.ports.size(); ++index) {
    if (!graph_port_equal(lhs.ports[index], rhs.ports[index])) {
      return false;
    }
  }

  for (size_t index = 0u; index < lhs.edges.size(); ++index) {
    if (!graph_edge_equal(lhs.edges[index], rhs.edges[index])) {
      return false;
    }
  }

  return true;
}

bool graph_target_equal(
    const std::optional<UIGraphSemanticTarget> &lhs,
    const std::optional<UIGraphSemanticTarget> &rhs
) {
  if (!lhs.has_value() || !rhs.has_value()) {
    return !lhs.has_value() && !rhs.has_value();
  }

  return lhs->semantic == rhs->semantic && lhs->primary_id == rhs->primary_id &&
         lhs->secondary_id == rhs->secondary_id;
}

bool contains_graph_id(const std::vector<UIGraphId> &ids, UIGraphId id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

void append_unique_graph_id(std::vector<UIGraphId> &ids, UIGraphId id) {
  if (!contains_graph_id(ids, id)) {
    ids.push_back(id);
  }
}

std::optional<UIGraphSemanticTarget>
graph_target_from_custom(const std::optional<UICustomHitData> &custom) {
  if (!custom.has_value()) {
    return std::nullopt;
  }

  return UIGraphSemanticTarget{
      .semantic = static_cast<UIGraphHitSemantic>(custom->semantic),
      .primary_id = custom->primary_id,
      .secondary_id = custom->secondary_id,
  };
}

bool is_left_button_event(const UIPointerEvent &event) {
  return event.button.has_value() &&
         *event.button == input::MouseButton::Left;
}

glm::vec2 graph_world_from_local(
    const UIDocument::UINode &node,
    glm::vec2 local_position
) {
  return screen_to_canvas(
      node.view_transform.value_or(UIViewTransform2D{}),
      local_position
  );
}

UIGraphNode *find_graph_node(UIGraphViewModel &model, UIGraphId node_id) {
  auto it = std::find_if(
      model.nodes.begin(),
      model.nodes.end(),
      [node_id](const UIGraphNode &node) { return node.id == node_id; }
  );
  return it != model.nodes.end() ? &*it : nullptr;
}

const UIGraphNode *find_graph_node(
    const UIGraphViewModel &model,
    UIGraphId node_id
) {
  auto it = std::find_if(
      model.nodes.begin(),
      model.nodes.end(),
      [node_id](const UIGraphNode &node) { return node.id == node_id; }
  );
  return it != model.nodes.end() ? &*it : nullptr;
}

const UIGraphPort *find_graph_port(
    const UIGraphViewModel &model,
    UIGraphId port_id
) {
  auto it = std::find_if(
      model.ports.begin(),
      model.ports.end(),
      [port_id](const UIGraphPort &port) { return port.id == port_id; }
  );
  return it != model.ports.end() ? &*it : nullptr;
}

const UIGraphEdge *find_graph_edge(
    const UIGraphViewModel &model,
    UIGraphId edge_id
) {
  auto it = std::find_if(
      model.edges.begin(),
      model.edges.end(),
      [edge_id](const UIGraphEdge &edge) { return edge.id == edge_id; }
  );
  return it != model.edges.end() ? &*it : nullptr;
}

bool graph_target_exists(
    const UIGraphViewModel &model,
    const std::optional<UIGraphSemanticTarget> &target
) {
  if (!target.has_value()) {
    return false;
  }

  switch (target->semantic) {
    case UIGraphHitSemantic::Background:
    case UIGraphHitSemantic::Marquee:
      return true;
    case UIGraphHitSemantic::NodeBody:
    case UIGraphHitSemantic::NodeHeader:
      return find_graph_node(model, target->primary_id) != nullptr;
    case UIGraphHitSemantic::Port:
      return find_graph_port(model, target->primary_id) != nullptr &&
             find_graph_node(model, target->secondary_id) != nullptr;
    case UIGraphHitSemantic::Edge:
      return find_graph_edge(model, target->primary_id) != nullptr;
    case UIGraphHitSemantic::None:
    default:
      return false;
  }
}

bool graph_drag_is_valid(
    const UIGraphViewModel &model,
    const UIGraphDragState &drag
) {
  switch (drag.mode) {
    case UIGraphDragMode::None:
    case UIGraphDragMode::Marquee:
      return true;
    case UIGraphDragMode::NodeMove:
      return find_graph_node(model, drag.primary_id) != nullptr;
    case UIGraphDragMode::ConnectionDrag:
      return find_graph_port(model, drag.primary_id) != nullptr;
    default:
      return false;
  }
}

std::optional<UIGraphId>
drop_target_port_id(const UIGraphViewState &state, const UIPointerEvent &event) {
  std::optional<UIGraphId> to_port_id =
      state.connection_preview.has_value()
          ? state.connection_preview->hovered_port_id
          : std::nullopt;
  if (!to_port_id.has_value() && event.custom.has_value()) {
    const auto target = graph_target_from_custom(event.custom);
    if (target.has_value() && target->semantic == UIGraphHitSemantic::Port) {
      to_port_id = target->primary_id;
    }
  }

  if (state.connection_preview.has_value() && to_port_id == state.connection_preview->from_port_id) {
    return std::nullopt;
  }

  return to_port_id;
}

void apply_graph_selection(
    UIDocument &document,
    UINodeId node_id,
    UIGraphSelection selection
) {
  auto *node = document.node(node_id);
  if (node == nullptr) {
    return;
  }

  if (graph_selection_equal(node->graph_view.model.selection, selection)) {
    return;
  }

  node->graph_view.model.selection = std::move(selection);
  document.mark_paint_dirty();

  if (node->graph_view.on_selection_change) {
    node->graph_view.on_selection_change(node->graph_view.model.selection);
  }
}

void set_graph_hovered_target(
    UIDocument &document,
    UINodeId node_id,
    std::optional<UIGraphSemanticTarget> target
) {
  auto *node = document.node(node_id);
  if (node == nullptr ||
      graph_target_equal(node->graph_view.hovered_target, target)) {
    return;
  }

  node->graph_view.hovered_target = std::move(target);
  document.mark_paint_dirty();
}

void set_graph_pressed_target(
    UIDocument &document,
    UINodeId node_id,
    std::optional<UIGraphSemanticTarget> target
) {
  auto *node = document.node(node_id);
  if (node == nullptr ||
      graph_target_equal(node->graph_view.pressed_target, target)) {
    return;
  }

  node->graph_view.pressed_target = std::move(target);
  document.mark_paint_dirty();
}

UIGraphSelection selection_for_target(
    const UIGraphSelection &current,
    const UIGraphSemanticTarget &target,
    bool extend
) {
  UIGraphSelection selection = extend ? current : UIGraphSelection{};
  switch (target.semantic) {
    case UIGraphHitSemantic::NodeBody:
    case UIGraphHitSemantic::NodeHeader:
      if (!extend) {
        selection.edge_ids.clear();
      }
      append_unique_graph_id(selection.node_ids, target.primary_id);
      break;
    case UIGraphHitSemantic::Edge:
      if (!extend) {
        selection.node_ids.clear();
      }
      append_unique_graph_id(selection.edge_ids, target.primary_id);
      break;
    default:
      break;
  }
  return selection;
}

void finalize_marquee_selection(
    UIDocument &document,
    UINodeId node_id,
    bool extend
) {
  auto *node = document.node(node_id);
  if (node == nullptr) {
    return;
  }

  const UIRect marquee_world = UIRect{
      .x = std::min(
          node->graph_view.marquee_start_world.x,
          node->graph_view.marquee_current_world.x
      ),
      .y = std::min(
          node->graph_view.marquee_start_world.y,
          node->graph_view.marquee_current_world.y
      ),
      .width = std::abs(
          node->graph_view.marquee_current_world.x -
          node->graph_view.marquee_start_world.x
      ),
      .height = std::abs(
          node->graph_view.marquee_current_world.y -
          node->graph_view.marquee_start_world.y
      ),
  };

  UIGraphSelection selection =
      extend ? node->graph_view.model.selection : UIGraphSelection{};
  if (!extend) {
    selection.edge_ids.clear();
  }

  for (const auto &layout_info : node->layout.graph_view.node_layouts) {
    if (intersects(marquee_world, layout_info.world_bounds)) {
      append_unique_graph_id(selection.node_ids, layout_info.id);
    }
  }

  apply_graph_selection(document, node_id, std::move(selection));
}

void begin_graph_drag(
    UIDocument &document,
    UINodeId node_id,
    const UIPointerEvent &event
) {
  auto *node = document.node(node_id);
  if (node == nullptr || !node->graph_view.pressed_target.has_value()) {
    return;
  }

  auto &state = node->graph_view;
  const UIGraphSemanticTarget pressed = *state.pressed_target;
  const glm::vec2 world = graph_world_from_local(*node, event.local_position);
  state.drag.press_world_position = world;
  state.drag.current_world_position = world;

  switch (pressed.semantic) {
    case UIGraphHitSemantic::NodeHeader: {
      auto *graph_node = find_graph_node(state.model, pressed.primary_id);
      if (graph_node == nullptr) {
        return;
      }
      state.drag.mode = UIGraphDragMode::NodeMove;
      state.drag.primary_id = pressed.primary_id;
      state.drag.origin_world_position = graph_node->position;
      document.mark_paint_dirty();
      break;
    }
    case UIGraphHitSemantic::Background:
      state.drag.mode = UIGraphDragMode::Marquee;
      state.marquee_visible = true;
      state.marquee_start_world = world;
      state.marquee_current_world = world;
      document.mark_paint_dirty();
      break;
    case UIGraphHitSemantic::Port:
      state.drag.mode = UIGraphDragMode::ConnectionDrag;
      state.drag.primary_id = pressed.primary_id;
      state.drag.secondary_id = pressed.secondary_id;
      state.connection_preview = UIGraphConnectionPreview{
          .from_port_id = pressed.primary_id,
          .hovered_port_id = std::nullopt,
          .current_world_position = world,
      };
      document.mark_paint_dirty();
      break;
    default:
      break;
  }
}

void update_graph_drag(
    UIDocument &document,
    UINodeId node_id,
    const UIPointerEvent &event
) {
  auto *node = document.node(node_id);
  if (node == nullptr) {
    return;
  }

  auto &state = node->graph_view;
  const glm::vec2 world = graph_world_from_local(*node, event.local_position);
  state.drag.current_world_position = world;

  switch (state.drag.mode) {
    case UIGraphDragMode::NodeMove: {
      auto *graph_node = find_graph_node(state.model, state.drag.primary_id);
      if (graph_node == nullptr) {
        return;
      }

      const glm::vec2 next_position =
          state.drag.origin_world_position +
          (world - state.drag.press_world_position);
      if (vec2_equal(graph_node->position, next_position)) {
        return;
      }

      graph_node->position = next_position;
      document.mark_layout_dirty();

      if (state.on_node_move) {
        state.on_node_move(graph_node->id, next_position);
      }
      break;
    }
    case UIGraphDragMode::Marquee:
      state.marquee_current_world = world;
      document.mark_paint_dirty();
      break;
    case UIGraphDragMode::ConnectionDrag: {
      if (!state.connection_preview.has_value()) {
        return;
      }

      state.connection_preview->current_world_position = world;
      std::optional<UIGraphId> hovered_port_id = std::nullopt;
      if (event.custom.has_value()) {
        const auto target = graph_target_from_custom(event.custom);
        if (target.has_value() && target->semantic == UIGraphHitSemantic::Port &&
            target->primary_id != state.connection_preview->from_port_id) {
          hovered_port_id = target->primary_id;
        }
      }
      state.connection_preview->hovered_port_id = hovered_port_id;
      document.mark_paint_dirty();
      break;
    }
    case UIGraphDragMode::None:
    default:
      break;
  }
}

void end_graph_drag(
    UIDocument &document,
    UINodeId node_id,
    const UIPointerEvent &event
) {
  auto *node = document.node(node_id);
  if (node == nullptr) {
    return;
  }

  auto &state = node->graph_view;
  switch (state.drag.mode) {
    case UIGraphDragMode::Marquee:
      finalize_marquee_selection(document, node_id, event.modifiers.shift);
      state.marquee_visible = false;
      state.drag = {};
      document.mark_paint_dirty();
      break;
    case UIGraphDragMode::ConnectionDrag: {
      const std::optional<UIGraphId> to_port_id =
          drop_target_port_id(state, event);
      if (state.on_connection_drag_end && state.connection_preview.has_value()) {
        state.on_connection_drag_end(
            state.connection_preview->from_port_id,
            to_port_id
        );
      }
      state.connection_preview.reset();
      state.drag = {};
      document.mark_paint_dirty();
      break;
    }
    case UIGraphDragMode::NodeMove:
      state.drag = {};
      document.mark_paint_dirty();
      break;
    case UIGraphDragMode::None:
    default:
      break;
  }
}

void handle_graph_pointer_event(
    UIDocument &document,
    UINodeId node_id,
    const UIPointerEvent &event
) {
  auto *node = document.node(node_id);
  if (node == nullptr || node->type != NodeType::GraphView) {
    return;
  }

  const std::optional<UIGraphSemanticTarget> target =
      graph_target_from_custom(event.custom);
  if (event.phase == UIPointerEventPhase::Move ||
      event.phase == UIPointerEventPhase::Press ||
      event.phase == UIPointerEventPhase::DragStart ||
      event.phase == UIPointerEventPhase::DragUpdate ||
      event.phase == UIPointerEventPhase::Release) {
    set_graph_hovered_target(document, node_id, target);
  }

  if (event.phase == UIPointerEventPhase::Move || !is_left_button_event(event)) {
    if (event.phase == UIPointerEventPhase::Release) {
      set_graph_pressed_target(document, node_id, std::nullopt);
      document.release_pointer_capture(node_id, input::MouseButton::Left);
    }
    return;
  }

  switch (event.phase) {
    case UIPointerEventPhase::Press:
      set_graph_pressed_target(document, node_id, target);
      if (target.has_value() &&
          (target->semantic == UIGraphHitSemantic::NodeHeader ||
           target->semantic == UIGraphHitSemantic::Background ||
           target->semantic == UIGraphHitSemantic::Port)) {
        document.request_pointer_capture(node_id, input::MouseButton::Left);
      }

      if (target.has_value() &&
          (target->semantic == UIGraphHitSemantic::NodeBody ||
           target->semantic == UIGraphHitSemantic::NodeHeader ||
           target->semantic == UIGraphHitSemantic::Edge)) {
        apply_graph_selection(
            document,
            node_id,
            selection_for_target(
                node->graph_view.model.selection,
                *target,
                event.modifiers.shift
            )
        );
      }
      break;
    case UIPointerEventPhase::DragStart:
      begin_graph_drag(document, node_id, event);
      break;
    case UIPointerEventPhase::DragUpdate:
      update_graph_drag(document, node_id, event);
      break;
    case UIPointerEventPhase::DragEnd:
      end_graph_drag(document, node_id, event);
      break;
    case UIPointerEventPhase::Release:
      if (node->graph_view.pressed_target.has_value() &&
          node->graph_view.pressed_target->semantic ==
              UIGraphHitSemantic::Background &&
          node->graph_view.drag.mode == UIGraphDragMode::None &&
          glm::length(event.total_delta) < k_click_distance_threshold) {
        apply_graph_selection(document, node_id, UIGraphSelection{});
      }
      set_graph_pressed_target(document, node_id, std::nullopt);
      document.release_pointer_capture(node_id, input::MouseButton::Left);
      break;
    case UIPointerEventPhase::Move:
      break;
  }
}

} // namespace

UINodeId UIDocument::create_graph_view() {
  const UINodeId node_id = allocate_node(NodeType::GraphView);

  mutate_style(node_id, [](UIStyle &style) { style.overflow = Overflow::Hidden; });
  set_view_transform_middle_mouse_pan(node_id, true);
  set_view_transform_wheel_zoom(node_id, true);
  set_on_custom_hit_test(
      node_id,
      [this, node_id](glm::vec2 local_position) -> std::optional<UICustomHitData> {
        const auto *node = this->node(node_id);
        if (node == nullptr || node->type != NodeType::GraphView) {
          return std::nullopt;
        }

        return hit_test_graph_view(*node, local_position);
      }
  );
  set_on_pointer_event(
      node_id,
      [this, node_id](const UIPointerEvent &event) {
        handle_graph_pointer_event(*this, node_id, event);
      }
  );

  return node_id;
}

void UIDocument::set_graph_view_model(UINodeId node_id, UIGraphViewModel model) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (graph_model_equal(target->graph_view.model, model)) {
    return;
  }

  std::optional<UIGraphSemanticTarget> hovered_target =
      graph_target_exists(model, target->graph_view.hovered_target)
          ? target->graph_view.hovered_target
          : std::nullopt;
  std::optional<UIGraphSemanticTarget> pressed_target =
      graph_target_exists(model, target->graph_view.pressed_target)
          ? target->graph_view.pressed_target
          : std::nullopt;
  UIGraphDragState drag =
      graph_drag_is_valid(model, target->graph_view.drag)
          ? target->graph_view.drag
          : UIGraphDragState{};
  const bool marquee_visible =
      target->graph_view.marquee_visible &&
      drag.mode == UIGraphDragMode::Marquee;
  std::optional<UIGraphConnectionPreview> connection_preview;
  if (target->graph_view.connection_preview.has_value()) {
    const auto &existing_preview = *target->graph_view.connection_preview;
    if (find_graph_port(model, existing_preview.from_port_id) != nullptr) {
      connection_preview = existing_preview;
      if (connection_preview->hovered_port_id.has_value() &&
          find_graph_port(model, *connection_preview->hovered_port_id) ==
              nullptr) {
        connection_preview->hovered_port_id.reset();
      }
    }
  }
  if (drag.mode == UIGraphDragMode::ConnectionDrag &&
      !connection_preview.has_value()) {
    drag = {};
  }

  target->graph_view.model = std::move(model);
  target->graph_view.hovered_target = std::move(hovered_target);
  target->graph_view.pressed_target = std::move(pressed_target);
  target->graph_view.drag = drag;
  target->graph_view.marquee_visible = marquee_visible;
  target->graph_view.connection_preview = std::move(connection_preview);
  mark_layout_dirty();
}

void UIDocument::set_on_graph_selection_change(
    UINodeId node_id,
    std::function<void(const UIGraphSelection &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->graph_view.on_selection_change = std::move(callback);
  }
}

void UIDocument::set_on_graph_node_move(
    UINodeId node_id,
    std::function<void(UIGraphId node_id, glm::vec2 position)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->graph_view.on_node_move = std::move(callback);
  }
}

void UIDocument::set_on_graph_connection_drag_end(
    UINodeId node_id,
    std::function<void(UIGraphId from_port_id, std::optional<UIGraphId>)>
        callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->graph_view.on_connection_drag_end = std::move(callback);
  }
}

} // namespace astralix::ui
