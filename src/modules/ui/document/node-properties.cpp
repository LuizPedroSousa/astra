#include "document.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace astralix::ui {

namespace {

bool vec4_equal(const glm::vec4 &lhs, const glm::vec4 &rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

bool line_chart_series_equal(
    const UILineChartSeries &lhs,
    const UILineChartSeries &rhs
) {
  return lhs.values == rhs.values && vec4_equal(lhs.color, rhs.color) &&
         lhs.thickness == rhs.thickness;
}

bool line_chart_series_equal(
    const std::vector<UILineChartSeries> &lhs,
    const std::vector<UILineChartSeries> &rhs
) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t index = 0u; index < lhs.size(); ++index) {
    if (!line_chart_series_equal(lhs[index], rhs[index])) {
      return false;
    }
  }

  return true;
}

} // namespace

void UIDocument::set_style(UINodeId node_id, const UIStyle &style) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  target->style = style;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::mutate_style(
    UINodeId node_id,
    const std::function<void(UIStyle &)> &fn
) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  fn(target->style);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_texture(
    UINodeId node_id,
    ResourceDescriptorID texture_id
) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (target->texture_id == texture_id) {
    return;
  }

  target->texture_id = std::move(texture_id);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_render_image_key(
    UINodeId node_id,
    RenderImageExportKey render_image_key
) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (target->render_image_key.has_value() &&
      *target->render_image_key == render_image_key) {
    return;
  }

  target->render_image_key = render_image_key;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_visible(UINodeId node_id, bool visible) {
  UINode *target = node(node_id);
  if (target == nullptr || target->visible == visible) {
    return;
  }

  if (!visible && target->type == NodeType::Popover) {
    close_popover(node_id);
    return;
  }

  if (!visible && target->type == NodeType::Select) {
    set_select_open(node_id, false);
  } else if (!visible && target->type == NodeType::Combobox) {
    set_combobox_open(node_id, false);
  }

  target->visible = visible;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_enabled(UINodeId node_id, bool enabled) {
  UINode *target = node(node_id);
  if (target == nullptr || target->enabled == enabled) {
    return;
  }

  target->enabled = enabled;
  if (!enabled && target->type == NodeType::Popover) {
    close_popover(node_id);
  } else if (!enabled && target->type == NodeType::Select) {
    set_select_open(node_id, false);
  } else if (!enabled && target->type == NodeType::Combobox) {
    set_combobox_open(node_id, false);
  }
  if (!enabled) {
    if (m_hot_node == node_id) {
      set_hot_node(k_invalid_node_id);
    }

    if (m_active_node == node_id) {
      set_active_node(k_invalid_node_id);
    }

    if (m_focused_node == node_id) {
      set_focused_node(k_invalid_node_id);
    }
  }

  m_paint_dirty = true;
}

void UIDocument::set_focusable(UINodeId node_id, bool focusable) {
  UINode *target = node(node_id);
  if (target == nullptr || target->focusable == focusable) {
    return;
  }

  target->focusable = focusable;
  if (!focusable && target->type == NodeType::Select) {
    set_select_open(node_id, false);
  } else if (!focusable && target->type == NodeType::Combobox) {
    set_combobox_open(node_id, false);
  }
  if (!focusable && m_focused_node == node_id) {
    set_focused_node(k_invalid_node_id);
  }
}

void UIDocument::set_scroll_offset(UINodeId node_id, glm::vec2 offset) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (target->layout.scroll.offset == offset) {
    return;
  }

  target->layout.scroll.offset = offset;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_line_chart_series(
    UINodeId node_id,
    std::vector<UILineChartSeries> series
) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (line_chart_series_equal(target->line_chart.series, series)) {
    return;
  }

  target->line_chart.series = std::move(series);

  if (target->line_chart.auto_range && !target->line_chart.series.empty()) {
    float computed_min = std::numeric_limits<float>::max();
    float computed_max = std::numeric_limits<float>::lowest();

    for (const auto &chart_series : target->line_chart.series) {
      for (float value : chart_series.values) {
        computed_min = std::min(computed_min, value);
        computed_max = std::max(computed_max, value);
      }
    }

    if (computed_min < computed_max) {
      target->line_chart.y_min = computed_min;
      target->line_chart.y_max = computed_max;
    }
  }

  m_paint_dirty = true;
}

void UIDocument::set_line_chart_range(
    UINodeId node_id, float y_min, float y_max
) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (target->line_chart.y_min == y_min && target->line_chart.y_max == y_max &&
      !target->line_chart.auto_range) {
    return;
  }

  target->line_chart.y_min = y_min;
  target->line_chart.y_max = y_max;
  target->line_chart.auto_range = false;
  m_paint_dirty = true;
}

void UIDocument::set_line_chart_auto_range(UINodeId node_id, bool auto_range) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (target->line_chart.auto_range == auto_range) {
    return;
  }

  target->line_chart.auto_range = auto_range;
  m_paint_dirty = true;
}

} // namespace astralix::ui
