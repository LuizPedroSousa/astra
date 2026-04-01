#pragma once

#include "dsl/core.hpp"

namespace astralix::ui::dsl {

struct LineChartStyle {
  float height = 120.0f;
  size_t grid_line_count = 4u;
  glm::vec4 grid_color = glm::vec4(1.0f, 1.0f, 1.0f, 0.1f);
  glm::vec4 background_color = glm::vec4(0.0f);
  float border_radius = 4.0f;
};

inline NodeSpec line_chart(const LineChartStyle &chart_style) {
  NodeSpec spec;
  spec.kind = NodeKind::LineChart;
  spec.line_chart_grid_line_count = chart_style.grid_line_count;
  spec.line_chart_grid_color = chart_style.grid_color;
  spec.style(
      styles::height(UILength::pixels(chart_style.height)),
      styles::width(UILength::percent(1.0f)),
      styles::background(chart_style.background_color),
      styles::radius(chart_style.border_radius),
      styles::overflow_hidden()
  );
  return spec;
}

namespace detail {

inline UINodeId
create_line_chart_node(UIDocument &document, const NodeSpec &) {
  return document.create_line_chart();
}

} // namespace detail
} // namespace astralix::ui::dsl
