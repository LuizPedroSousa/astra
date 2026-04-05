#pragma once

#include "dsl/core.hpp"
#include "dsl/widgets/actions/pressable.hpp"
#include "dsl/widgets/layout/view.hpp"

#include <glm/glm.hpp>

namespace astralix::ui::dsl {

struct BarChartStyle {
  float height = 80.0f;
  float bar_gap = 1.0f;
  glm::vec4 bar_color = glm::vec4(1.0f);
  glm::vec4 bar_background = glm::vec4(0.0f);
  float border_radius = 2.0f;
};

inline NodeSpec bar_chart(const BarChartStyle &chart_style) {
  return view()
      .style(
          styles::fill_x()
              .flex_row()
              .items_end()
              .gap(chart_style.bar_gap)
              .height(UILength::pixels(chart_style.height))
              .radius(6.0f)
              .background(chart_style.bar_background)
              .overflow_hidden()
      );
}

inline NodeSpec bar_chart_bar(
    const glm::vec4 &fill_color,
    float border_radius
) {
  return pressable()
      .style(
          styles::flex(1.0f)
              .height(UILength::pixels(0.0f))
              .self_end()
              .radius(border_radius)
              .background(fill_color)
      );
}

} // namespace astralix::ui::dsl
