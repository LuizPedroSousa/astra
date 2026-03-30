#include "layout/widgets/inputs/slider.hpp"

#include "layout/common.hpp"

#include <algorithm>

namespace astralix::ui {

glm::vec2
measure_slider_size(const UIDocument::UINode &node, const UILayoutContext &) {
  const float thumb_diameter =
      std::max(8.0f, node.style.slider_thumb_radius * 2.0f);
  const float track_thickness =
      std::max(2.0f, node.style.slider_track_thickness);
  return glm::vec2(
      180.0f,
      node.style.padding.vertical() + std::max(thumb_diameter, track_thickness)
  );
}

void update_slider_layout(UIDocument::UINode &node) {
  node.layout.slider = UILayoutMetrics::SliderLayout{};

  const UIRect content = node.layout.content_bounds;
  const float thumb_radius = std::max(4.0f, node.style.slider_thumb_radius);
  const float track_thickness =
      std::max(2.0f, node.style.slider_track_thickness);
  const float track_x = content.x + thumb_radius;
  const float track_width = std::max(0.0f, content.width - thumb_radius * 2.0f);
  const float track_y =
      content.y + std::max(0.0f, (content.height - track_thickness) * 0.5f);
  const float span = node.slider.max_value - node.slider.min_value;
  const float ratio =
      span > 0.0f ? (node.slider.value - node.slider.min_value) / span : 0.0f;
  const float clamped_ratio = std::clamp(ratio, 0.0f, 1.0f);
  const float thumb_center_x = track_x + track_width * clamped_ratio;
  const float thumb_diameter = thumb_radius * 2.0f;
  const float thumb_y =
      content.y + std::max(0.0f, (content.height - thumb_diameter) * 0.5f);

  node.layout.slider.track_rect = UIRect{
      .x = track_x,
      .y = track_y,
      .width = track_width,
      .height = track_thickness,
  };
  node.layout.slider.fill_rect = UIRect{
      .x = track_x,
      .y = track_y,
      .width = std::max(0.0f, thumb_center_x - track_x),
      .height = track_thickness,
  };
  node.layout.slider.thumb_rect = UIRect{
      .x = thumb_center_x - thumb_radius,
      .y = thumb_y,
      .width = thumb_diameter,
      .height = thumb_diameter,
  };
}

void append_slider_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
) {
  const UIRect track = node.layout.slider.track_rect;
  const UIRect fill = node.layout.slider.fill_rect;
  const UIRect thumb = node.layout.slider.thumb_rect;

  auto append_rect = [&](const UIRect &rect,
                         glm::vec4 color,
                         float radius,
                         glm::vec4 border_color = glm::vec4(0.0f),
                         float border_width = 0.0f) {
    if (rect.width <= 0.0f || rect.height <= 0.0f || color.a <= 0.0f) {
      return;
    }

    UIDrawCommand command;
    command.type = DrawCommandType::Rect;
    command.node_id = node_id;
    command.rect = rect;
    apply_content_clip(command, node);
    command.color = color * glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    command.border_color =
        border_color * glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    command.border_width = border_width;
    command.border_radius = radius;
    document.draw_list().commands.push_back(std::move(command));
  };

  append_rect(
      track,
      glm::vec4(0.184f, 0.204f, 0.255f, 0.92f),
      track.height * 0.5f
  );
  append_rect(fill, node.style.accent_color, track.height * 0.5f);

  glm::vec4 thumb_color = node.style.accent_color;
  if (node.paint_state.pressed) {
    thumb_color = glm::mix(thumb_color, glm::vec4(1.0f), 0.2f);
  } else if (node.paint_state.hovered || node.paint_state.focused) {
    thumb_color = glm::mix(thumb_color, glm::vec4(1.0f), 0.1f);
  }

  append_rect(
      thumb,
      thumb_color,
      thumb.width * 0.5f,
      glm::vec4(0.141f, 0.157f, 0.200f, 0.96f),
      1.0f
  );
}

} // namespace astralix::ui
