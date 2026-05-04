#include "layout/widgets/graph/graph-view.hpp"

#include "canvas/view-transform.hpp"
#include "layout/common.hpp"
#include "vector/path-builder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

namespace astralix::ui {
namespace {

constexpr float k_graph_min_width = 320.0f;
constexpr float k_graph_min_height = 240.0f;
constexpr float k_graph_extent_padding = 80.0f;
constexpr float k_header_height = 30.0f;
constexpr float k_node_min_width = 176.0f;
constexpr float k_node_empty_body_height = 18.0f;
constexpr float k_port_row_height = 24.0f;
constexpr float k_socket_radius = 6.0f;
constexpr float k_socket_inset = 14.0f;
constexpr float k_node_header_padding_x = 14.0f;
constexpr float k_node_body_padding_x = 14.0f;
constexpr float k_port_label_gap = 10.0f;
constexpr float k_port_column_gap = 28.0f;
constexpr float k_grid_minor_spacing = 24.0f;
constexpr float k_grid_major_spacing = 120.0f;
constexpr float k_edge_control_min = 42.0f;
constexpr float k_edge_control_max = 180.0f;
constexpr float k_edge_hit_tolerance = 6.0f;
constexpr float k_socket_min_screen_radius = 6.0f;

constexpr glm::vec4 k_grid_minor_color = glm::vec4(0.52f, 0.61f, 0.76f, 0.10f);
constexpr glm::vec4 k_grid_major_color = glm::vec4(0.62f, 0.74f, 0.92f, 0.18f);
constexpr glm::vec4 k_node_body_color = glm::vec4(0.11f, 0.14f, 0.19f, 0.98f);
constexpr glm::vec4 k_node_header_color = glm::vec4(0.16f, 0.20f, 0.27f, 1.0f);
constexpr glm::vec4 k_node_hover_outline = glm::vec4(0.74f, 0.85f, 1.0f, 0.52f);
constexpr glm::vec4 k_node_selected_outline = glm::vec4(0.47f, 0.72f, 1.0f, 0.95f);
constexpr glm::vec4 k_edge_hover_color = glm::vec4(0.85f, 0.91f, 1.0f, 1.0f);
constexpr glm::vec4 k_socket_outline_color = glm::vec4(0.06f, 0.08f, 0.11f, 0.92f);
constexpr glm::vec4 k_marquee_fill_color = glm::vec4(0.41f, 0.67f, 1.0f, 0.14f);
constexpr glm::vec4 k_marquee_stroke_color = glm::vec4(0.54f, 0.78f, 1.0f, 0.96f);
constexpr glm::vec4 k_preview_edge_color = glm::vec4(0.56f, 0.82f, 1.0f, 0.88f);

struct MeasuredGraphNode {
  float width = k_node_min_width;
  float height = k_header_height + k_node_empty_body_height;
  size_t row_count = 0u;
};

struct NodeMeasureCache {
  std::unordered_map<UIGraphId, MeasuredGraphNode> values;
};

const UIGraphPort *find_port(
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

const UIGraphPortLayoutInfo *find_port_layout(
    const UIGraphViewLayoutInfo &layout,
    UIGraphId port_id
) {
  auto it = std::find_if(
      layout.port_layouts.begin(),
      layout.port_layouts.end(),
      [port_id](const UIGraphPortLayoutInfo &port) { return port.id == port_id; }
  );
  return it != layout.port_layouts.end() ? &*it : nullptr;
}

const UIGraphNodeLayoutInfo *find_node_layout(
    const UIGraphViewLayoutInfo &layout,
    UIGraphId node_id
) {
  auto it = std::find_if(
      layout.node_layouts.begin(),
      layout.node_layouts.end(),
      [node_id](const UIGraphNodeLayoutInfo &item) { return item.id == node_id; }
  );
  return it != layout.node_layouts.end() ? &*it : nullptr;
}

bool selection_contains(
    const std::vector<UIGraphId> &ids,
    UIGraphId id
) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

float graph_zoom(const UIDocument::UINode &node) {
  const UIViewTransform2D transform =
      node.view_transform.value_or(UIViewTransform2D{});
  return clamp_zoom(transform, transform.zoom);
}

UIViewTransform2D graph_transform(const UIDocument::UINode &node) {
  return node.view_transform.value_or(UIViewTransform2D{});
}

glm::vec2 local_screen_from_world(
    const UIDocument::UINode &node,
    glm::vec2 world_position
) {
  return canvas_to_screen(graph_transform(node), world_position);
}

glm::vec2 absolute_screen_from_world(
    const UIDocument::UINode &node,
    glm::vec2 world_position
) {
  return canvas_to_screen(
      graph_transform(node),
      world_position,
      glm::vec2(node.layout.content_bounds.x, node.layout.content_bounds.y)
  );
}

glm::vec2 world_from_local_screen(
    const UIDocument::UINode &node,
    glm::vec2 local_screen_position
) {
  return screen_to_canvas(graph_transform(node), local_screen_position);
}

UIRect normalized_rect(glm::vec2 a, glm::vec2 b) {
  const float min_x = std::min(a.x, b.x);
  const float min_y = std::min(a.y, b.y);
  const float max_x = std::max(a.x, b.x);
  const float max_y = std::max(a.y, b.y);
  return UIRect{
      .x = min_x,
      .y = min_y,
      .width = max_x - min_x,
      .height = max_y - min_y,
  };
}

UIRect screen_rect_from_world(
    const UIDocument::UINode &node,
    const UIRect &world_rect
) {
  const glm::vec2 top_left = absolute_screen_from_world(
      node,
      glm::vec2(world_rect.x, world_rect.y)
  );
  const glm::vec2 bottom_right = absolute_screen_from_world(
      node,
      glm::vec2(world_rect.right(), world_rect.bottom())
  );
  return normalized_rect(top_left, bottom_right);
}

float edge_control_offset(float delta_x) {
  return std::clamp(std::abs(delta_x) * 0.5f, k_edge_control_min, k_edge_control_max);
}

glm::vec2 cubic_point(
    glm::vec2 p0,
    glm::vec2 p1,
    glm::vec2 p2,
    glm::vec2 p3,
    float t
) {
  const float u = 1.0f - t;
  return u * u * u * p0 + 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 +
         t * t * t * p3;
}

float distance_to_segment(glm::vec2 point, glm::vec2 a, glm::vec2 b) {
  const glm::vec2 ab = b - a;
  const float length_squared = glm::dot(ab, ab);
  if (length_squared <= 0.0001f) {
    return glm::length(point - a);
  }

  const float t = std::clamp(glm::dot(point - a, ab) / length_squared, 0.0f, 1.0f);
  const glm::vec2 closest = a + ab * t;
  return glm::length(point - closest);
}

float distance_to_cubic(
    glm::vec2 point,
    glm::vec2 p0,
    glm::vec2 p1,
    glm::vec2 p2,
    glm::vec2 p3
) {
  constexpr size_t k_segments = 24u;
  float best = std::numeric_limits<float>::max();
  glm::vec2 previous = p0;
  for (size_t index = 1u; index <= k_segments; ++index) {
    const float t = static_cast<float>(index) / static_cast<float>(k_segments);
    const glm::vec2 current = cubic_point(p0, p1, p2, p3, t);
    best = std::min(best, distance_to_segment(point, previous, current));
    previous = current;
  }
  return best;
}

MeasuredGraphNode measure_graph_node(
    const UIDocument::UINode &graph_view,
    const UILayoutContext &context,
    const UIGraphViewModel &model,
    const UIGraphNode &graph_node
) {
  float max_input_label_width = 0.0f;
  float max_output_label_width = 0.0f;
  size_t input_count = 0u;
  size_t output_count = 0u;

  for (UIGraphId port_id : graph_node.input_ports) {
    const auto *port = find_port(model, port_id);
    if (port == nullptr) {
      continue;
    }
    ++input_count;
    max_input_label_width = std::max(
        max_input_label_width,
        measure_label_width(graph_view, context, port->label)
    );
  }

  for (UIGraphId port_id : graph_node.output_ports) {
    const auto *port = find_port(model, port_id);
    if (port == nullptr) {
      continue;
    }
    ++output_count;
    max_output_label_width = std::max(
        max_output_label_width,
        measure_label_width(graph_view, context, port->label)
    );
  }

  const size_t row_count = std::max(input_count, output_count);
  const float title_width =
      measure_label_width(graph_view, context, graph_node.title);
  const float ports_width =
      k_node_body_padding_x * 2.0f + k_socket_inset * 2.0f +
      k_socket_radius * 4.0f + max_input_label_width + max_output_label_width +
      k_port_column_gap + k_port_label_gap * 2.0f;

  MeasuredGraphNode measured;
  measured.row_count = row_count;
  measured.width = std::max(
      {
          k_node_min_width,
          title_width + k_node_header_padding_x * 2.0f,
          ports_width,
          std::max(0.0f, graph_node.size_hint.x),
      }
  );
  measured.height = std::max(
      k_header_height + (row_count > 0u
                             ? static_cast<float>(row_count) * k_port_row_height
                             : k_node_empty_body_height),
      std::max(0.0f, graph_node.size_hint.y)
  );
  return measured;
}

NodeMeasureCache measure_graph_nodes(
    const UIDocument::UINode &node,
    const UILayoutContext &context
) {
  NodeMeasureCache cache;
  cache.values.reserve(node.graph_view.model.nodes.size());
  for (const auto &graph_node : node.graph_view.model.nodes) {
    cache.values.emplace(
        graph_node.id,
        measure_graph_node(node, context, node.graph_view.model, graph_node)
    );
  }
  return cache;
}

bool graph_hover_visible(const UIDocument::UINode &node) {
  return node.paint_state.hovered ||
         node.graph_view.drag.mode == UIGraphDragMode::ConnectionDrag;
}

glm::vec4 selected_or_hovered_edge_color(
    const UIDocument::UINode &node,
    UIGraphId edge_id,
    glm::vec4 base_color
) {
  if (selection_contains(node.graph_view.model.selection.edge_ids, edge_id)) {
    return k_node_selected_outline;
  }

  if (graph_hover_visible(node) && node.graph_view.hovered_target.has_value() &&
      node.graph_view.hovered_target->semantic == UIGraphHitSemantic::Edge &&
      node.graph_view.hovered_target->primary_id == edge_id) {
    return k_edge_hover_color;
  }

  return base_color;
}

glm::vec4 node_outline_color(
    const UIDocument::UINode &node,
    UIGraphId node_id
) {
  if (selection_contains(node.graph_view.model.selection.node_ids, node_id)) {
    return k_node_selected_outline;
  }

  if (graph_hover_visible(node) && node.graph_view.hovered_target.has_value() &&
      (node.graph_view.hovered_target->semantic == UIGraphHitSemantic::NodeBody ||
       node.graph_view.hovered_target->semantic ==
           UIGraphHitSemantic::NodeHeader) &&
      node.graph_view.hovered_target->primary_id == node_id) {
    return k_node_hover_outline;
  }

  return glm::vec4(0.16f, 0.21f, 0.28f, 0.92f);
}

void append_grid_paths(
    UIDrawCommand &command,
    const UIDocument::UINode &node
) {
  const UIViewTransform2D transform = graph_transform(node);
  const glm::vec2 local_extent(
      node.layout.content_bounds.width,
      node.layout.content_bounds.height
  );
  const glm::vec2 world_a = screen_to_canvas(transform, glm::vec2(0.0f));
  const glm::vec2 world_b = screen_to_canvas(transform, local_extent);
  const float min_x = std::min(world_a.x, world_b.x);
  const float max_x = std::max(world_a.x, world_b.x);
  const float min_y = std::min(world_a.y, world_b.y);
  const float max_y = std::max(world_a.y, world_b.y);

  auto build_grid = [&](float spacing, glm::vec4 color) {
    UIPathBuilder builder(UIPathStyle{
        .fill = false,
        .stroke = true,
        .stroke_color = color,
        .stroke_width = 1.0f,
        .line_cap = UIStrokeCap::Butt,
    });

    const float start_x = std::floor(min_x / spacing) * spacing;
    for (float x = start_x; x <= max_x + 0.001f; x += spacing) {
      builder.move_to(absolute_screen_from_world(node, glm::vec2(x, min_y)));
      builder.line_to(absolute_screen_from_world(node, glm::vec2(x, max_y)));
    }

    const float start_y = std::floor(min_y / spacing) * spacing;
    for (float y = start_y; y <= max_y + 0.001f; y += spacing) {
      builder.move_to(absolute_screen_from_world(node, glm::vec2(min_x, y)));
      builder.line_to(absolute_screen_from_world(node, glm::vec2(max_x, y)));
    }

    command.path_commands.push_back(builder.release());
  };

  build_grid(k_grid_minor_spacing, k_grid_minor_color);
  build_grid(k_grid_major_spacing, k_grid_major_color);
}

} // namespace

glm::vec2 measure_graph_view_size(
    const UIDocument::UINode &node,
    const UILayoutContext &context
) {
  const auto cache = measure_graph_nodes(node, context);
  if (node.graph_view.model.nodes.empty()) {
    return glm::vec2(
        k_graph_min_width + node.style.padding.horizontal(),
        k_graph_min_height + node.style.padding.vertical()
    );
  }

  float min_x = std::numeric_limits<float>::max();
  float min_y = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float max_y = std::numeric_limits<float>::lowest();

  for (const auto &graph_node : node.graph_view.model.nodes) {
    auto it = cache.values.find(graph_node.id);
    if (it == cache.values.end()) {
      continue;
    }

    min_x = std::min(min_x, graph_node.position.x);
    min_y = std::min(min_y, graph_node.position.y);
    max_x = std::max(max_x, graph_node.position.x + it->second.width);
    max_y = std::max(max_y, graph_node.position.y + it->second.height);
  }

  if (min_x == std::numeric_limits<float>::max()) {
    return glm::vec2(
        k_graph_min_width + node.style.padding.horizontal(),
        k_graph_min_height + node.style.padding.vertical()
    );
  }

  return glm::vec2(
      std::max(k_graph_min_width, (max_x - min_x) + k_graph_extent_padding * 2.0f) +
          node.style.padding.horizontal(),
      std::max(k_graph_min_height, (max_y - min_y) + k_graph_extent_padding * 2.0f) +
          node.style.padding.vertical()
  );
}

void update_graph_view_layout(
    UIDocument::UINode &node,
    const UILayoutContext &context
) {
  auto &layout = node.layout.graph_view;
  layout.node_layouts.clear();
  layout.port_layouts.clear();
  layout.edge_layouts.clear();

  const auto cache = measure_graph_nodes(node, context);
  const auto &model = node.graph_view.model;

  for (const auto &graph_node : model.nodes) {
    auto measure_it = cache.values.find(graph_node.id);
    if (measure_it == cache.values.end()) {
      continue;
    }

    const MeasuredGraphNode &measured = measure_it->second;
    const UIRect world_bounds{
        .x = graph_node.position.x,
        .y = graph_node.position.y,
        .width = measured.width,
        .height = measured.height,
    };
    layout.node_layouts.push_back(UIGraphNodeLayoutInfo{
        .id = graph_node.id,
        .world_bounds = world_bounds,
        .header_world_bounds =
            UIRect{
                .x = world_bounds.x,
                .y = world_bounds.y,
                .width = world_bounds.width,
                .height = k_header_height,
            },
    });

    for (size_t index = 0u; index < graph_node.input_ports.size(); ++index) {
      const auto *port = find_port(model, graph_node.input_ports[index]);
      if (port == nullptr) {
        continue;
      }

      const float row_y =
          graph_node.position.y + k_header_height +
          static_cast<float>(index) * k_port_row_height;
      layout.port_layouts.push_back(UIGraphPortLayoutInfo{
          .id = port->id,
          .node_id = graph_node.id,
          .direction = UIGraphPortDirection::Input,
          .row_world_bounds =
              UIRect{
                  .x = world_bounds.x,
                  .y = row_y,
                  .width = world_bounds.width,
                  .height = k_port_row_height,
              },
          .socket_world_center =
              glm::vec2(
                  graph_node.position.x + k_socket_inset + k_socket_radius,
                  row_y + k_port_row_height * 0.5f
              ),
          .socket_world_radius = k_socket_radius,
          .label_width = measure_label_width(node, context, port->label),
      });
    }

    for (size_t index = 0u; index < graph_node.output_ports.size(); ++index) {
      const auto *port = find_port(model, graph_node.output_ports[index]);
      if (port == nullptr) {
        continue;
      }

      const float row_y =
          graph_node.position.y + k_header_height +
          static_cast<float>(index) * k_port_row_height;
      layout.port_layouts.push_back(UIGraphPortLayoutInfo{
          .id = port->id,
          .node_id = graph_node.id,
          .direction = UIGraphPortDirection::Output,
          .row_world_bounds =
              UIRect{
                  .x = world_bounds.x,
                  .y = row_y,
                  .width = world_bounds.width,
                  .height = k_port_row_height,
              },
          .socket_world_center =
              glm::vec2(
                  world_bounds.right() - k_socket_inset - k_socket_radius,
                  row_y + k_port_row_height * 0.5f
              ),
          .socket_world_radius = k_socket_radius,
          .label_width = measure_label_width(node, context, port->label),
      });
    }
  }

  for (const auto &edge : model.edges) {
    const auto *from = find_port_layout(layout, edge.from_port_id);
    const auto *to = find_port_layout(layout, edge.to_port_id);
    if (from == nullptr || to == nullptr) {
      continue;
    }

    const float control = edge_control_offset(
        to->socket_world_center.x - from->socket_world_center.x
    );
    layout.edge_layouts.push_back(UIGraphEdgeLayoutInfo{
        .id = edge.id,
        .from_port_id = edge.from_port_id,
        .to_port_id = edge.to_port_id,
        .start_world = from->socket_world_center,
        .control_a_world =
            glm::vec2(
                from->socket_world_center.x + control,
                from->socket_world_center.y
            ),
        .control_b_world =
            glm::vec2(
                to->socket_world_center.x - control,
                to->socket_world_center.y
            ),
        .end_world = to->socket_world_center,
        .color = edge.color,
        .thickness = edge.thickness,
    });
  }
}

void append_graph_view_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved
) {
  if (node.layout.content_bounds.width <= 0.0f ||
      node.layout.content_bounds.height <= 0.0f) {
    return;
  }

  UIDrawCommand grid_command;
  grid_command.type = DrawCommandType::Path;
  grid_command.node_id = node_id;
  grid_command.rect = node.layout.content_bounds;
  apply_content_clip(grid_command, node);
  append_grid_paths(grid_command, node);
  document.draw_list().commands.push_back(std::move(grid_command));

  const float zoom = graph_zoom(node);
  const float line_height = measure_line_height(node, context) * zoom;
  const ResourceDescriptorID &font_id = resolve_ui_font_id(node, context);
  const float font_size = resolve_ui_font_size(node, context) * zoom;

  UIDrawCommand edge_command;
  edge_command.type = DrawCommandType::Path;
  edge_command.node_id = node_id;
  edge_command.rect = node.layout.content_bounds;
  apply_content_clip(edge_command, node);

  for (const auto &edge : node.layout.graph_view.edge_layouts) {
    UIPathBuilder builder(UIPathStyle{
        .fill = false,
        .stroke = true,
        .stroke_color = selected_or_hovered_edge_color(node, edge.id, edge.color),
        .stroke_width = std::max(1.0f, edge.thickness * zoom),
        .line_cap = UIStrokeCap::Round,
        .line_join = UIStrokeJoin::Round,
    });
    builder.move_to(absolute_screen_from_world(node, edge.start_world))
        .cubic_to(
            absolute_screen_from_world(node, edge.control_a_world),
            absolute_screen_from_world(node, edge.control_b_world),
            absolute_screen_from_world(node, edge.end_world)
        );
    edge_command.path_commands.push_back(builder.release());
  }

  if (!edge_command.path_commands.empty()) {
    document.draw_list().commands.push_back(std::move(edge_command));
  }

  for (const auto &graph_node : node.graph_view.model.nodes) {
    const auto *layout_info = find_node_layout(node.layout.graph_view, graph_node.id);
    if (layout_info == nullptr) {
      continue;
    }

    const UIRect screen_bounds = screen_rect_from_world(node, layout_info->world_bounds);
    const UIRect screen_header_bounds =
        screen_rect_from_world(node, layout_info->header_world_bounds);

    UIDrawCommand body_command;
    body_command.type = DrawCommandType::Rect;
    body_command.node_id = node_id;
    body_command.rect = screen_bounds;
    apply_content_clip(body_command, node);
    body_command.color = k_node_body_color;
    body_command.border_color = node_outline_color(node, graph_node.id);
    body_command.border_width = selection_contains(
                                    node.graph_view.model.selection.node_ids,
                                    graph_node.id
                                )
                                    ? 2.0f
                                    : 1.0f;
    body_command.border_radius = 10.0f * zoom;
    document.draw_list().commands.push_back(std::move(body_command));

    UIDrawCommand header_command;
    header_command.type = DrawCommandType::Rect;
    header_command.node_id = node_id;
    header_command.rect = screen_header_bounds;
    apply_content_clip(header_command, node);
    header_command.color = k_node_header_color;
    header_command.border_color = glm::vec4(0.0f);
    document.draw_list().commands.push_back(std::move(header_command));

    UIDrawCommand title_command;
    title_command.type = DrawCommandType::Text;
    title_command.node_id = node_id;
    title_command.rect = screen_header_bounds;
    apply_content_clip(title_command, node);
    title_command.text_origin = glm::vec2(
        screen_header_bounds.x + k_node_header_padding_x * zoom,
        screen_header_bounds.y +
            std::max(0.0f, (screen_header_bounds.height - line_height) * 0.5f)
    );
    title_command.text = graph_node.title;
    title_command.font_id = font_id;
    title_command.font_size = font_size;
    title_command.color = glm::vec4(
        resolved.text_color.r,
        resolved.text_color.g,
        resolved.text_color.b,
        resolved.opacity
    );
    document.draw_list().commands.push_back(std::move(title_command));
  }

  UIDrawCommand socket_command;
  socket_command.type = DrawCommandType::Path;
  socket_command.node_id = node_id;
  socket_command.rect = node.layout.content_bounds;
  apply_content_clip(socket_command, node);

  for (const auto &graph_node : node.graph_view.model.nodes) {
    for (UIGraphId port_id : graph_node.input_ports) {
      const auto *port = find_port(node.graph_view.model, port_id);
      const auto *layout_info = find_port_layout(node.layout.graph_view, port_id);
      if (port == nullptr || layout_info == nullptr) {
        continue;
      }

      UIDrawCommand label_command;
      label_command.type = DrawCommandType::Text;
      label_command.node_id = node_id;
      label_command.rect =
          screen_rect_from_world(node, layout_info->row_world_bounds);
      apply_content_clip(label_command, node);
      label_command.text_origin = glm::vec2(
          absolute_screen_from_world(
              node,
              glm::vec2(
                  layout_info->socket_world_center.x + k_socket_radius +
                      k_port_label_gap,
                  layout_info->row_world_bounds.y
              )
          ).x,
          absolute_screen_from_world(
              node,
              glm::vec2(layout_info->row_world_bounds.x, layout_info->row_world_bounds.y)
          ).y +
              std::max(0.0f, (layout_info->row_world_bounds.height * zoom - line_height) * 0.5f)
      );
      label_command.text = port->label;
      label_command.font_id = font_id;
      label_command.font_size = font_size;
      label_command.color = glm::vec4(
          resolved.text_color.r,
          resolved.text_color.g,
          resolved.text_color.b,
          resolved.opacity
      );
      document.draw_list().commands.push_back(std::move(label_command));

      const bool hovered =
          graph_hover_visible(node) &&
          node.graph_view.hovered_target.has_value() &&
          node.graph_view.hovered_target->semantic == UIGraphHitSemantic::Port &&
          node.graph_view.hovered_target->primary_id == port->id;
      UIPathBuilder builder(UIPathStyle{
          .fill = true,
          .stroke = true,
          .fill_color = port->color,
          .stroke_color = hovered ? k_edge_hover_color : k_socket_outline_color,
          .stroke_width = hovered ? 2.0f : 1.2f,
      });
      builder.append_circle(
          absolute_screen_from_world(node, layout_info->socket_world_center),
          layout_info->socket_world_radius * zoom
      );
      socket_command.path_commands.push_back(builder.release());
    }

    for (UIGraphId port_id : graph_node.output_ports) {
      const auto *port = find_port(node.graph_view.model, port_id);
      const auto *layout_info = find_port_layout(node.layout.graph_view, port_id);
      if (port == nullptr || layout_info == nullptr) {
        continue;
      }

      UIDrawCommand label_command;
      label_command.type = DrawCommandType::Text;
      label_command.node_id = node_id;
      label_command.rect =
          screen_rect_from_world(node, layout_info->row_world_bounds);
      apply_content_clip(label_command, node);
      const float text_x =
          absolute_screen_from_world(
              node,
              glm::vec2(
                  layout_info->row_world_bounds.right() - k_socket_inset -
                      k_socket_radius * 2.0f - k_port_label_gap,
                  layout_info->row_world_bounds.y
              )
          ).x -
          layout_info->label_width * zoom;
      label_command.text_origin = glm::vec2(
          text_x,
          absolute_screen_from_world(
              node,
              glm::vec2(layout_info->row_world_bounds.x, layout_info->row_world_bounds.y)
          ).y +
              std::max(0.0f, (layout_info->row_world_bounds.height * zoom - line_height) * 0.5f)
      );
      label_command.text = port->label;
      label_command.font_id = font_id;
      label_command.font_size = font_size;
      label_command.color = glm::vec4(
          resolved.text_color.r,
          resolved.text_color.g,
          resolved.text_color.b,
          resolved.opacity
      );
      document.draw_list().commands.push_back(std::move(label_command));

      const bool hovered =
          graph_hover_visible(node) &&
          node.graph_view.hovered_target.has_value() &&
          node.graph_view.hovered_target->semantic == UIGraphHitSemantic::Port &&
          node.graph_view.hovered_target->primary_id == port->id;
      UIPathBuilder builder(UIPathStyle{
          .fill = true,
          .stroke = true,
          .fill_color = port->color,
          .stroke_color = hovered ? k_edge_hover_color : k_socket_outline_color,
          .stroke_width = hovered ? 2.0f : 1.2f,
      });
      builder.append_circle(
          absolute_screen_from_world(node, layout_info->socket_world_center),
          layout_info->socket_world_radius * zoom
      );
      socket_command.path_commands.push_back(builder.release());
    }
  }

  if (!socket_command.path_commands.empty()) {
    document.draw_list().commands.push_back(std::move(socket_command));
  }

  if (node.graph_view.connection_preview.has_value()) {
    const auto *from = find_port_layout(
        node.layout.graph_view,
        node.graph_view.connection_preview->from_port_id
    );
    if (from != nullptr) {
      const glm::vec2 end_world =
          node.graph_view.connection_preview->current_world_position;
      const float control = edge_control_offset(
          end_world.x - from->socket_world_center.x
      );
      UIDrawCommand preview_command;
      preview_command.type = DrawCommandType::Path;
      preview_command.node_id = node_id;
      preview_command.rect = node.layout.content_bounds;
      apply_content_clip(preview_command, node);
      UIPathBuilder builder(UIPathStyle{
          .fill = false,
          .stroke = true,
          .stroke_color = k_preview_edge_color,
          .stroke_width = std::max(1.0f, 2.0f * zoom),
      });
      builder.move_to(absolute_screen_from_world(node, from->socket_world_center))
          .cubic_to(
              absolute_screen_from_world(
                  node,
                  glm::vec2(
                      from->socket_world_center.x + control,
                      from->socket_world_center.y
                  )
              ),
              absolute_screen_from_world(
                  node,
                  glm::vec2(end_world.x - control, end_world.y)
              ),
              absolute_screen_from_world(node, end_world)
          );
      preview_command.path_commands.push_back(builder.release());
      document.draw_list().commands.push_back(std::move(preview_command));
    }
  }

  if (node.graph_view.marquee_visible) {
    UIDrawCommand marquee_command;
    marquee_command.type = DrawCommandType::Path;
    marquee_command.node_id = node_id;
    marquee_command.rect = node.layout.content_bounds;
    apply_content_clip(marquee_command, node);
    UIPathBuilder builder(UIPathStyle{
        .fill = true,
        .stroke = true,
        .fill_color = k_marquee_fill_color,
        .stroke_color = k_marquee_stroke_color,
        .stroke_width = 1.5f,
        .line_cap = UIStrokeCap::Butt,
        .line_join = UIStrokeJoin::Miter,
    });
    builder.append_rect(
        screen_rect_from_world(
            node,
            normalized_rect(
                node.graph_view.marquee_start_world,
                node.graph_view.marquee_current_world
            )
        )
    );
    marquee_command.path_commands.push_back(builder.release());
    document.draw_list().commands.push_back(std::move(marquee_command));
  }
}

std::optional<UICustomHitData> hit_test_graph_view(
    const UIDocument::UINode &node,
    glm::vec2 local_position
) {
  const glm::vec2 world_position = world_from_local_screen(node, local_position);

  for (auto it = node.layout.graph_view.port_layouts.rbegin();
       it != node.layout.graph_view.port_layouts.rend();
       ++it) {
    const glm::vec2 local_center =
        local_screen_from_world(node, it->socket_world_center);
    const float radius = std::max(
        k_socket_min_screen_radius,
        it->socket_world_radius * graph_zoom(node)
    );
    if (glm::length(local_position - local_center) <= radius + 3.0f) {
      return UICustomHitData{
          .semantic = static_cast<uint32_t>(UIGraphHitSemantic::Port),
          .primary_id = it->id,
          .secondary_id = it->node_id,
      };
    }
  }

  for (auto it = node.layout.graph_view.edge_layouts.rbegin();
       it != node.layout.graph_view.edge_layouts.rend();
       ++it) {
    const float distance = distance_to_cubic(
        local_position,
        local_screen_from_world(node, it->start_world),
        local_screen_from_world(node, it->control_a_world),
        local_screen_from_world(node, it->control_b_world),
        local_screen_from_world(node, it->end_world)
    );
    const float tolerance =
        std::max(k_edge_hit_tolerance, it->thickness * graph_zoom(node));
    if (distance <= tolerance) {
      return UICustomHitData{
          .semantic = static_cast<uint32_t>(UIGraphHitSemantic::Edge),
          .primary_id = it->id,
      };
    }
  }

  for (auto it = node.layout.graph_view.node_layouts.rbegin();
       it != node.layout.graph_view.node_layouts.rend();
       ++it) {
    if (it->header_world_bounds.contains(world_position)) {
      return UICustomHitData{
          .semantic = static_cast<uint32_t>(UIGraphHitSemantic::NodeHeader),
          .primary_id = it->id,
      };
    }

    if (it->world_bounds.contains(world_position)) {
      return UICustomHitData{
          .semantic = static_cast<uint32_t>(UIGraphHitSemantic::NodeBody),
          .primary_id = it->id,
      };
    }
  }

  if (node.graph_view.marquee_visible) {
    const UIRect marquee_world = normalized_rect(
        node.graph_view.marquee_start_world,
        node.graph_view.marquee_current_world
    );
    if (marquee_world.contains(world_position)) {
      return UICustomHitData{
          .semantic = static_cast<uint32_t>(UIGraphHitSemantic::Marquee),
      };
    }
  }

  return UICustomHitData{
      .semantic = static_cast<uint32_t>(UIGraphHitSemantic::Background),
  };
}

} // namespace astralix::ui
