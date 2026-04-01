#include "layout/widgets/data-viz/line-chart.hpp"

#include "layout/common.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace astralix::ui {

void append_line_chart_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
) {
  const UIRect content_bounds = node.layout.content_bounds;
  if (content_bounds.width <= 0.0f || content_bounds.height <= 0.0f) {
    return;
  }

  const auto &chart_state = node.line_chart;

  float y_min = chart_state.y_min;
  float y_max = chart_state.y_max;

  if (chart_state.auto_range && !chart_state.series.empty()) {
    y_min = std::numeric_limits<float>::max();
    y_max = std::numeric_limits<float>::lowest();

    for (const auto &series : chart_state.series) {
      for (float value : series.values) {
        y_min = std::min(y_min, value);
        y_max = std::max(y_max, value);
      }
    }

    if (y_min == y_max) {
      y_min -= 1.0f;
      y_max += 1.0f;
    }

    const float padding = (y_max - y_min) * 0.05f;
    y_min -= padding;
    y_max += padding;
  }

  const float y_range = y_max - y_min;
  if (y_range <= 0.0f) {
    return;
  }

  if (chart_state.grid_line_count > 0u && chart_state.grid_color.a > 0.0f) {
    for (size_t i = 0u; i <= chart_state.grid_line_count; ++i) {
      const float ratio =
          static_cast<float>(i) / static_cast<float>(chart_state.grid_line_count);
      const float grid_y =
          content_bounds.y + content_bounds.height - ratio * content_bounds.height;

      UIDrawCommand grid_command;
      grid_command.type = DrawCommandType::Rect;
      grid_command.node_id = node_id;
      grid_command.rect = UIRect{
          .x = content_bounds.x,
          .y = grid_y,
          .width = content_bounds.width,
          .height = 1.0f,
      };
      apply_content_clip(grid_command, node);
      grid_command.color = chart_state.grid_color;
      document.draw_list().commands.push_back(std::move(grid_command));
    }
  }

  UIDrawCommand polyline_command;
  polyline_command.type = DrawCommandType::Polyline;
  polyline_command.node_id = node_id;
  polyline_command.rect = content_bounds;
  apply_content_clip(polyline_command, node);

  for (const auto &series : chart_state.series) {
    if (series.values.size() < 2u) {
      continue;
    }

    UIPolylineSeries polyline_series;
    polyline_series.thickness = series.thickness;
    polyline_series.vertices.reserve(series.values.size());

    const float x_step =
        content_bounds.width / static_cast<float>(series.values.size() - 1u);

    for (size_t i = 0u; i < series.values.size(); ++i) {
      const float normalized =
          (series.values[i] - y_min) / y_range;
      const float pixel_x =
          content_bounds.x + static_cast<float>(i) * x_step;
      const float pixel_y =
          content_bounds.y + content_bounds.height -
          normalized * content_bounds.height;

      polyline_series.vertices.push_back(UIPolylineVertex{
          .position = glm::vec2(pixel_x, pixel_y),
          .color = series.color,
      });
    }

    polyline_command.polyline_series.push_back(std::move(polyline_series));
  }

  if (!polyline_command.polyline_series.empty()) {
    document.draw_list().commands.push_back(std::move(polyline_command));
  }
}

} // namespace astralix::ui
