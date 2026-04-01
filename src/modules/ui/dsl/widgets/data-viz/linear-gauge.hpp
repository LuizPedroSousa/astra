#pragma once

#include "dsl/core.hpp"
#include "dsl/widgets/layout/view.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace astralix::ui::dsl {

struct GaugeThreshold {
  float limit;
  glm::vec4 color;
};

struct LinearGaugeStyle {
  float height = 8.0f;
  float border_radius = 4.0f;
  glm::vec4 track_color;
  std::vector<GaugeThreshold> thresholds;
};

inline glm::vec4 gauge_color_for_ratio(
    float ratio,
    const std::vector<GaugeThreshold> &thresholds
) {
  if (thresholds.empty()) {
    return glm::vec4(1.0f);
  }

  for (const auto &threshold : thresholds) {
    if (ratio <= threshold.limit) {
      return threshold.color;
    }
  }

  return thresholds.back().color;
}

inline NodeSpec linear_gauge(
    const LinearGaugeStyle &gauge_style,
    UINodeId &fill_node
) {
  return view()
      .style(
          styles::fill_x()
              .flex_row()
              .items_start()
              .height(UILength::pixels(gauge_style.height))
              .radius(gauge_style.border_radius)
              .background(gauge_style.track_color)
              .overflow_hidden()
      )
      .children(
          view()
              .bind(fill_node)
              .style(
                  StyleBuilder{}
                      .height(UILength::percent(1.0f))
                      .width(UILength::percent(0.0f))
                      .radius(gauge_style.border_radius)
                      .background(
                          gauge_style.thresholds.empty()
                              ? glm::vec4(1.0f)
                              : gauge_style.thresholds.front().color
                      )
              )
      );
}

} // namespace astralix::ui::dsl
