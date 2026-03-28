#include "layout.hpp"

#include "assert.hpp"
#include "managers/resource-manager.hpp"
#include "resources/font.hpp"
#include "resources/texture.hpp"
#include "foundations.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>

namespace astralix::ui {
namespace {

struct MeasureContext {
  const UILayoutContext &context;
};

float clamp_dimension(
    float value, UILength min_value, UILength max_value, float basis,
    float rem_basis
) {
  if (min_value.unit != UiLengthUnit::Auto) {
    const float min_dimension =
        min_value.unit == UiLengthUnit::Percent ? basis * min_value.value
        : min_value.unit == UiLengthUnit::Rem   ? rem_basis * min_value.value
                                                : min_value.value;
    value = std::max(value, min_dimension);
  }

  if (max_value.unit != UiLengthUnit::Auto) {
    const float max_dimension =
        max_value.unit == UiLengthUnit::Percent ? basis * max_value.value
        : max_value.unit == UiLengthUnit::Rem   ? rem_basis * max_value.value
                                                : max_value.value;
    value = std::min(value, max_dimension);
  }

  return std::max(0.0f, value);
}

float resolve_length(
    UILength value, float basis, float rem_basis, float auto_value = 0.0f
) {
  switch (value.unit) {
    case UiLengthUnit::Pixels:
      return value.value;
    case UiLengthUnit::Percent:
      return basis * value.value;
    case UiLengthUnit::Rem:
      return rem_basis * value.value;
    case UiLengthUnit::Auto:
    default:
      return auto_value;
  }
}

ResourceDescriptorID resolve_font_id(
    const UIDocument::UINode &node, const UILayoutContext &context
) {
  return node.style.font_id.empty() ? context.default_font_id
                                    : node.style.font_id;
}

float resolve_font_size(
    const UIDocument::UINode &node, const UILayoutContext &context
) {
  return node.style.font_size > 0.0f ? node.style.font_size
                                     : context.default_font_size;
}

Ref<Font>
resolve_font(const UIDocument::UINode &node, const UILayoutContext &context) {
  const auto font_id = resolve_font_id(node, context);
  if (font_id.empty()) {
    return nullptr;
  }

  return resource_manager()->get_by_descriptor_id<Font>(font_id);
}

float glyph_advance(const Font &font, char character, uint32_t pixel_size) {
  auto glyph = font.glyph(character, pixel_size);
  if (!glyph.has_value()) {
    return 0.0f;
  }

  return static_cast<float>(glyph->advance >> 6);
}

float measure_text_width(
    const Font &font, std::string_view text, uint32_t pixel_size
) {
  float width = 0.0f;
  for (const char character : text) {
    width += glyph_advance(font, character, pixel_size);
  }

  return width;
}

glm::vec2 measure_text_input_size(
    const UIDocument::UINode &node, const UILayoutContext &context
) {
  auto font = resolve_font(node, context);
  const float font_size = resolve_font_size(node, context);
  const uint32_t resolved_font_size =
      static_cast<uint32_t>(std::max(1.0f, std::round(font_size)));

  float text_width = 0.0f;
  float line_height = font_size;
  if (font != nullptr) {
    text_width = std::max(
        measure_text_width(*font, node.text, resolved_font_size),
        measure_text_width(*font, node.placeholder, resolved_font_size)
    );
    line_height = font->line_height(font_size);
  }

  return glm::vec2(
      text_width + node.style.padding.horizontal(),
      line_height + node.style.padding.vertical()
  );
}

float measure_label_width(
    const UIDocument::UINode &node, const UILayoutContext &context,
    std::string_view text
) {
  auto font = resolve_font(node, context);
  const float font_size = resolve_font_size(node, context);
  const uint32_t resolved_font_size =
      static_cast<uint32_t>(std::max(1.0f, std::round(font_size)));
  if (font == nullptr) {
    return static_cast<float>(text.size()) * font_size * 0.5f;
  }

  return measure_text_width(*font, text, resolved_font_size);
}

float measure_line_height(
    const UIDocument::UINode &node, const UILayoutContext &context
) {
  auto font = resolve_font(node, context);
  const float font_size = resolve_font_size(node, context);
  return font != nullptr ? font->line_height(font_size) : font_size;
}

constexpr float k_segmented_item_padding_x = 14.0f;
constexpr float k_segmented_item_padding_y = 8.0f;
constexpr float k_chip_item_padding_x = 12.0f;
constexpr float k_chip_item_padding_y = 7.0f;

std::string_view select_display_text(const UIDocument::UINode &node) {
  if (!node.select.options.empty() &&
      node.select.selected_index < node.select.options.size()) {
    return node.select.options[node.select.selected_index];
  }

  return node.placeholder;
}

void apply_self_clip(UiDrawCommand &command, const UIDocument::UINode &node) {
  command.has_clip = node.layout.has_clip;
  command.clip_rect = node.layout.clip_bounds;
}

void apply_content_clip(
    UiDrawCommand &command, const UIDocument::UINode &node
) {
  command.has_clip = node.layout.has_content_clip;
  command.clip_rect = node.layout.content_clip_bounds;
}

std::optional<UiRect> child_clip_rect(const UIDocument::UINode &node) {
  if (!node.layout.has_content_clip) {
    return std::nullopt;
  }

  return node.layout.content_clip_bounds;
}

glm::vec2 measure_checkbox_size(
    const UIDocument::UINode &node, const UILayoutContext &context
) {
  const float indicator = std::max(8.0f, node.style.control_indicator_size);
  const float line_height = measure_line_height(node, context);
  const float label_width =
      node.text.empty() ? 0.0f : measure_label_width(node, context, node.text);
  const float gap = node.text.empty() ? 0.0f : node.style.control_gap;

  return glm::vec2(
      node.style.padding.horizontal() + indicator + gap + label_width,
      node.style.padding.vertical() + std::max(indicator, line_height)
  );
}

glm::vec2
measure_slider_size(const UIDocument::UINode &node, const UILayoutContext &) {
  const float thumb_diameter =
      std::max(8.0f, node.style.slider_thumb_radius * 2.0f);
  const float track_thickness =
      std::max(2.0f, node.style.slider_track_thickness);
  return glm::vec2(
      180.0f + node.style.padding.horizontal(),
      node.style.padding.vertical() + std::max(thumb_diameter, track_thickness)
  );
}

glm::vec2 measure_select_size(
    const UIDocument::UINode &node, const UILayoutContext &context
) {
  float label_width = measure_label_width(node, context, node.placeholder);
  for (const auto &option : node.select.options) {
    label_width =
        std::max(label_width, measure_label_width(node, context, option));
  }

  const float line_height = measure_line_height(node, context);
  const float indicator = std::max(12.0f, node.style.control_indicator_size);
  return glm::vec2(
      node.style.padding.horizontal() + label_width + node.style.control_gap +
          indicator,
      node.style.padding.vertical() + std::max(line_height, indicator)
  );
}

glm::vec2 measure_segmented_control_size(
    const UIDocument::UINode &node, const UILayoutContext &context
) {
  const float line_height = measure_line_height(node, context);
  const float item_height = line_height + k_segmented_item_padding_y * 2.0f;
  float total_width = node.style.padding.horizontal();
  for (size_t index = 0u; index < node.segmented_control.options.size(); ++index) {
    total_width += measure_label_width(
        node, context, node.segmented_control.options[index]
    ) + k_segmented_item_padding_x * 2.0f;
    if (index + 1u < node.segmented_control.options.size()) {
      total_width += node.style.control_gap;
    }
  }

  return glm::vec2(total_width, node.style.padding.vertical() + item_height);
}

glm::vec2 measure_chip_group_size(
    const UIDocument::UINode &node, const UILayoutContext &context
) {
  const float line_height = measure_line_height(node, context);
  const float item_height = line_height + k_chip_item_padding_y * 2.0f;
  float total_width = node.style.padding.horizontal();
  for (size_t index = 0u; index < node.chip_group.options.size(); ++index) {
    total_width +=
        measure_label_width(node, context, node.chip_group.options[index]) +
        k_chip_item_padding_x * 2.0f;
    if (index + 1u < node.chip_group.options.size()) {
      total_width += node.style.control_gap;
    }
  }

  return glm::vec2(total_width, node.style.padding.vertical() + item_height);
}

UiRect
resolve_single_line_text_rect(const UiRect &content_bounds, float line_height) {
  const float resolved_height = std::max(content_bounds.height, line_height);
  const float y_offset =
      std::max(0.0f, (content_bounds.height - line_height) * 0.5f);

  return UiRect{
      .x = content_bounds.x,
      .y = content_bounds.y + y_offset,
      .width = content_bounds.width,
      .height = resolved_height,
  };
}

void update_checkbox_layout(
    UIDocument::UINode &node, const UILayoutContext &context
) {
  node.layout.checkbox = UiLayoutMetrics::CheckboxLayout{};

  const float indicator = std::max(8.0f, node.style.control_indicator_size);
  const float line_height = measure_line_height(node, context);
  const UiRect content = node.layout.content_bounds;
  const float indicator_y =
      content.y + std::max(0.0f, (content.height - indicator) * 0.5f);

  node.layout.checkbox.indicator_rect = UiRect{
      .x = content.x,
      .y = indicator_y,
      .width = indicator,
      .height = indicator,
  };

  const float label_x = node.layout.checkbox.indicator_rect.right() +
                        (node.text.empty() ? 0.0f : node.style.control_gap);
  node.layout.checkbox.label_rect = resolve_single_line_text_rect(
      UiRect{
          .x = label_x,
          .y = content.y,
          .width = std::max(0.0f, content.right() - label_x),
          .height = content.height,
      },
      line_height
  );
}

void update_slider_layout(UIDocument::UINode &node) {
  node.layout.slider = UiLayoutMetrics::SliderLayout{};

  const UiRect content = node.layout.content_bounds;
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

  node.layout.slider.track_rect = UiRect{
      .x = track_x,
      .y = track_y,
      .width = track_width,
      .height = track_thickness,
  };
  node.layout.slider.fill_rect = UiRect{
      .x = track_x,
      .y = track_y,
      .width = std::max(0.0f, thumb_center_x - track_x),
      .height = track_thickness,
  };
  node.layout.slider.thumb_rect = UiRect{
      .x = thumb_center_x - thumb_radius,
      .y = thumb_y,
      .width = thumb_diameter,
      .height = thumb_diameter,
  };
}

void update_select_layout(
    UIDocument::UINode &node, const UILayoutContext &context
) {
  node.layout.select = UiLayoutMetrics::SelectLayout{};
  if (!node.select.open || node.select.options.empty()) {
    return;
  }

  const float line_height = measure_line_height(node, context);
  const float row_height =
      std::max(line_height, node.style.control_indicator_size) + 10.0f;
  float popup_width = node.layout.bounds.width;
  for (const auto &option : node.select.options) {
    popup_width = std::max(
        popup_width, measure_label_width(node, context, option) +
                         node.style.padding.horizontal() +
                         node.style.control_indicator_size
    );
  }

  const float popup_height = row_height * node.select.options.size() + 8.0f;
  float popup_x = node.layout.bounds.x;
  float popup_y = node.layout.bounds.bottom() + 4.0f;

  if (popup_x + popup_width > context.viewport_size.x) {
    popup_x = std::max(0.0f, context.viewport_size.x - popup_width);
  }

  if (popup_y + popup_height > context.viewport_size.y) {
    popup_y = std::max(0.0f, node.layout.bounds.y - popup_height - 4.0f);
  }

  node.layout.select.popup_rect = UiRect{
      .x = popup_x,
      .y = popup_y,
      .width = popup_width,
      .height = popup_height,
  };

  node.layout.select.option_rects.reserve(node.select.options.size());
  for (size_t index = 0; index < node.select.options.size(); ++index) {
    node.layout.select.option_rects.push_back(
        UiRect{
            .x = popup_x + 4.0f,
            .y = popup_y + 4.0f + row_height * static_cast<float>(index),
            .width = std::max(0.0f, popup_width - 8.0f),
            .height = row_height,
        }
    );
  }
}

void update_segmented_control_layout(
    UIDocument::UINode &node, const UILayoutContext &context
) {
  node.layout.segmented_control = UiLayoutMetrics::SegmentedControlLayout{};
  const UiRect content = node.layout.content_bounds;
  if (content.width <= 0.0f || content.height <= 0.0f ||
      node.segmented_control.options.empty()) {
    return;
  }

  const float line_height = measure_line_height(node, context);
  const float item_height = std::min(
      content.height, line_height + k_segmented_item_padding_y * 2.0f
  );
  const float y =
      content.y + std::max(0.0f, (content.height - item_height) * 0.5f);

  float cursor_x = content.x;
  node.layout.segmented_control.item_rects.reserve(
      node.segmented_control.options.size()
  );
  for (size_t index = 0u; index < node.segmented_control.options.size();
       ++index) {
    const float item_width =
        measure_label_width(node, context, node.segmented_control.options[index]) +
        k_segmented_item_padding_x * 2.0f;
    node.layout.segmented_control.item_rects.push_back(UiRect{
        .x = cursor_x,
        .y = y,
        .width = item_width,
        .height = item_height,
    });
    cursor_x += item_width + node.style.control_gap;
  }
}

void update_chip_group_layout(UIDocument::UINode &node,
                              const UILayoutContext &context) {
  node.layout.chip_group = UiLayoutMetrics::ChipGroupLayout{};
  const UiRect content = node.layout.content_bounds;
  if (content.width <= 0.0f || content.height <= 0.0f ||
      node.chip_group.options.empty()) {
    return;
  }

  const float line_height = measure_line_height(node, context);
  const float item_height =
      std::min(content.height, line_height + k_chip_item_padding_y * 2.0f);
  const float y =
      content.y + std::max(0.0f, (content.height - item_height) * 0.5f);

  float cursor_x = content.x;
  node.layout.chip_group.item_rects.reserve(node.chip_group.options.size());
  for (size_t index = 0u; index < node.chip_group.options.size(); ++index) {
    const float item_width =
        measure_label_width(node, context, node.chip_group.options[index]) +
        k_chip_item_padding_x * 2.0f;
    node.layout.chip_group.item_rects.push_back(UiRect{
        .x = cursor_x,
        .y = y,
        .width = item_width,
        .height = item_height,
    });
    cursor_x += item_width + node.style.control_gap;
  }
}

void reset_scrollbar_geometry(UiScrollState &scroll) {
  scroll.vertical_scrollbar_visible = false;
  scroll.horizontal_scrollbar_visible = false;
  scroll.vertical_track_rect = UiRect{};
  scroll.vertical_thumb_rect = UiRect{};
  scroll.horizontal_track_rect = UiRect{};
  scroll.horizontal_thumb_rect = UiRect{};
  scroll.vertical_thumb_hovered = false;
  scroll.vertical_thumb_active = false;
  scroll.horizontal_thumb_hovered = false;
  scroll.horizontal_thumb_active = false;
}

void update_scrollbar_geometry(UIDocument::UINode &node) {
  auto &scroll = node.layout.scroll;
  reset_scrollbar_geometry(scroll);

  if (node.type != NodeType::ScrollView ||
      node.style.scrollbar_visibility == ScrollbarVisibility::Hidden) {
    return;
  }

  const float thickness = std::max(0.0f, node.style.scrollbar_thickness);
  if (thickness <= 0.0f) {
    return;
  }

  const bool wants_vertical = scrolls_vertically(node.style.scroll_mode);
  const bool wants_horizontal = scrolls_horizontally(node.style.scroll_mode);

  const bool show_vertical =
      wants_vertical &&
      (node.style.scrollbar_visibility == ScrollbarVisibility::Always ||
       scroll.max_offset.y > 0.0f);
  const bool show_horizontal =
      wants_horizontal &&
      (node.style.scrollbar_visibility == ScrollbarVisibility::Always ||
       scroll.max_offset.x > 0.0f);

  const UiRect viewport = node.layout.content_bounds;

  if (show_vertical) {
    const float track_height =
        std::max(0.0f, viewport.height - (show_horizontal ? thickness : 0.0f));
    if (track_height > 0.0f) {
      scroll.vertical_scrollbar_visible = true;
      scroll.vertical_track_rect = UiRect{
          .x = viewport.right() - thickness,
          .y = viewport.y,
          .width = thickness,
          .height = track_height,
      };

      const float content_height =
          std::max(scroll.content_size.y, scroll.viewport_size.y);
      const float thumb_height = std::clamp(
          content_height > 0.0f
              ? (scroll.viewport_size.y / content_height) * track_height
              : track_height,
          std::min(track_height, node.style.scrollbar_min_thumb_length),
          track_height
      );
      const float travel = std::max(0.0f, track_height - thumb_height);
      const float ratio = scroll.max_offset.y > 0.0f
                              ? scroll.offset.y / scroll.max_offset.y
                              : 0.0f;
      scroll.vertical_thumb_rect = UiRect{
          .x = scroll.vertical_track_rect.x,
          .y = scroll.vertical_track_rect.y + ratio * travel,
          .width = thickness,
          .height = thumb_height,
      };
    }
  }

  if (show_horizontal) {
    const float track_width =
        std::max(0.0f, viewport.width - (show_vertical ? thickness : 0.0f));
    if (track_width > 0.0f) {
      scroll.horizontal_scrollbar_visible = true;
      scroll.horizontal_track_rect = UiRect{
          .x = viewport.x,
          .y = viewport.bottom() - thickness,
          .width = track_width,
          .height = thickness,
      };

      const float content_width =
          std::max(scroll.content_size.x, scroll.viewport_size.x);
      const float thumb_width = std::clamp(
          content_width > 0.0f
              ? (scroll.viewport_size.x / content_width) * track_width
              : track_width,
          std::min(track_width, node.style.scrollbar_min_thumb_length),
          track_width
      );
      const float travel = std::max(0.0f, track_width - thumb_width);
      const float ratio = scroll.max_offset.x > 0.0f
                              ? scroll.offset.x / scroll.max_offset.x
                              : 0.0f;
      scroll.horizontal_thumb_rect = UiRect{
          .x = scroll.horizontal_track_rect.x + ratio * travel,
          .y = scroll.horizontal_track_rect.y,
          .width = thumb_width,
          .height = thickness,
      };
    }
  }
}

UiRect resize_handle_rect(const UIDocument::UINode &node, UIHitPart part) {
  const UiRect bounds = node.layout.bounds;
  const float thickness = std::max(1.0f, node.style.resize_handle_thickness);
  const float corner_extent =
      std::max(thickness, node.style.resize_corner_extent);

  switch (part) {
    case UIHitPart::ResizeLeft:
      return UiRect{.x = bounds.x,
                    .y = bounds.y,
                    .width = thickness,
                    .height = bounds.height};
    case UIHitPart::ResizeTop:
      return UiRect{.x = bounds.x,
                    .y = bounds.y,
                    .width = bounds.width,
                    .height = thickness};
    case UIHitPart::ResizeRight:
      return UiRect{.x = bounds.right() - thickness,
                    .y = bounds.y,
                    .width = thickness,
                    .height = bounds.height};
    case UIHitPart::ResizeBottom:
      return UiRect{.x = bounds.x,
                    .y = bounds.bottom() - thickness,
                    .width = bounds.width,
                    .height = thickness};
    case UIHitPart::ResizeTopLeft:
      return UiRect{.x = bounds.x,
                    .y = bounds.y,
                    .width = corner_extent,
                    .height = corner_extent};
    case UIHitPart::ResizeTopRight:
      return UiRect{.x = bounds.right() - corner_extent,
                    .y = bounds.y,
                    .width = corner_extent,
                    .height = corner_extent};
    case UIHitPart::ResizeBottomLeft:
      return UiRect{.x = bounds.x,
                    .y = bounds.bottom() - corner_extent,
                    .width = corner_extent,
                    .height = corner_extent};
    case UIHitPart::ResizeBottomRight:
      return UiRect{.x = bounds.right() - corner_extent,
                    .y = bounds.bottom() - corner_extent,
                    .width = corner_extent,
                    .height = corner_extent};
    default:
      return UiRect{};
  }
}

std::optional<UIHitPart>
hit_test_resize_handles(const UIDocument::UINode &node, glm::vec2 point) {
  if (!node_supports_panel_resize(node)) {
    return std::nullopt;
  }

  const bool allow_horizontal =
      resize_allows_horizontal(node.style.resize_mode);
  const bool allow_vertical = resize_allows_vertical(node.style.resize_mode);
  const bool left_enabled =
      allow_horizontal &&
      has_resize_edge(node.style.resize_edges, k_resize_edge_left);
  const bool top_enabled =
      allow_vertical &&
      has_resize_edge(node.style.resize_edges, k_resize_edge_top);
  const bool right_enabled =
      allow_horizontal &&
      has_resize_edge(node.style.resize_edges, k_resize_edge_right);
  const bool bottom_enabled =
      allow_vertical &&
      has_resize_edge(node.style.resize_edges, k_resize_edge_bottom);

  if (left_enabled && top_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeTopLeft).contains(point)) {
    return UIHitPart::ResizeTopLeft;
  }
  if (right_enabled && top_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeTopRight).contains(point)) {
    return UIHitPart::ResizeTopRight;
  }
  if (left_enabled && bottom_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeBottomLeft).contains(point)) {
    return UIHitPart::ResizeBottomLeft;
  }
  if (right_enabled && bottom_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeBottomRight).contains(point)) {
    return UIHitPart::ResizeBottomRight;
  }
  if (left_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeLeft).contains(point)) {
    return UIHitPart::ResizeLeft;
  }
  if (top_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeTop).contains(point)) {
    return UIHitPart::ResizeTop;
  }
  if (right_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeRight).contains(point)) {
    return UIHitPart::ResizeRight;
  }
  if (bottom_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeBottom).contains(point)) {
    return UIHitPart::ResizeBottom;
  }

  return std::nullopt;
}

void append_text_commands(
    UIDocument &document, UINodeId node_id, const UiRect &text_rect,
    const UIDocument::UINode &node, const UILayoutContext &context,
    const UIResolvedStyle &resolved, std::string_view text,
    glm::vec4 text_color, float text_scroll_x, bool draw_selection,
    bool draw_caret
) {
  if (text.empty()) {
    return;
  }

  auto font = resolve_font(node, context);
  const float font_size = resolve_font_size(node, context);
  if (font == nullptr) {
    return;
  }

  const uint32_t resolved_font_size =
      static_cast<uint32_t>(std::max(1.0f, std::round(font_size)));
  const glm::vec2 text_origin(text_rect.x - text_scroll_x, text_rect.y);

  if (draw_selection && !node.selection.empty()) {
    const float start_x = measure_text_prefix_advance(
        node.text, node.selection.start(), [&](char character) {
          return glyph_advance(*font, character, resolved_font_size);
        }
    );
    const float end_x = measure_text_prefix_advance(
        node.text, node.selection.end(), [&](char character) {
          return glyph_advance(*font, character, resolved_font_size);
        }
    );

    if (end_x > start_x) {
      UiDrawCommand selection_command;
      selection_command.type = DrawCommandType::Rect;
      selection_command.node_id = node_id;
      selection_command.rect = UiRect{
          .x = text_origin.x + start_x,
          .y = text_rect.y,
          .width = end_x - start_x,
          .height = std::max(text_rect.height, font->line_height(font_size))};
      apply_content_clip(selection_command, node);
      selection_command.color = resolved.selection_color;
      document.draw_list().commands.push_back(std::move(selection_command));
    }
  }

  UiDrawCommand command;
  command.type = DrawCommandType::Text;
  command.node_id = node_id;
  command.rect = text_rect;
  command.text_origin = text_origin;
  command.text = std::string(text);
  command.font_id = resolve_font_id(node, context);
  command.font_size = font_size;
  command.color = text_color;
  command.color.a *= resolved.opacity;
  apply_content_clip(command, node);
  document.draw_list().commands.push_back(std::move(command));

  if (draw_caret && node.caret.active && node.caret.visible) {
    const float caret_x = measure_text_prefix_advance(
        node.text, node.caret.index, [&](char character) {
          return glyph_advance(*font, character, resolved_font_size);
        }
    );

    UiDrawCommand caret_command;
    caret_command.type = DrawCommandType::Rect;
    caret_command.node_id = node_id;
    caret_command.rect = UiRect{
        .x = text_origin.x + caret_x,
        .y = text_rect.y,
        .width = 1.0f,
        .height = std::max(text_rect.height, font->line_height(font_size)),
    };
    apply_content_clip(caret_command, node);
    caret_command.color = resolved.caret_color;
    document.draw_list().commands.push_back(std::move(caret_command));
  }
}

void append_checkbox_commands(
    UIDocument &document, UINodeId node_id, const UIDocument::UINode &node,
    const UILayoutContext &context, const UIResolvedStyle &resolved
) {
  const UiRect indicator = node.layout.checkbox.indicator_rect;
  if (indicator.width > 0.0f && indicator.height > 0.0f) {
    UiDrawCommand indicator_command;
    indicator_command.type = DrawCommandType::Rect;
    indicator_command.node_id = node_id;
    indicator_command.rect = indicator;
    apply_content_clip(indicator_command, node);
    indicator_command.color = glm::vec4(0.02f, 0.05f, 0.1f, 0.92f) *
                              glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    indicator_command.border_color =
        (node.checkbox.checked ? node.style.accent_color
                               : resolved.border_color) *
        glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    indicator_command.border_width = 1.0f;
    indicator_command.border_radius = 5.0f;
    document.draw_list().commands.push_back(std::move(indicator_command));

    if (node.checkbox.checked) {
      const float inset = std::max(3.0f, indicator.width * 0.2f);
      UiDrawCommand mark_command;
      mark_command.type = DrawCommandType::Rect;
      mark_command.node_id = node_id;
      mark_command.rect = inset_rect(indicator, UIEdges::all(inset));
      apply_content_clip(mark_command, node);
      mark_command.color = node.style.accent_color *
                           glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
      mark_command.border_radius = 3.0f;
      document.draw_list().commands.push_back(std::move(mark_command));
    }
  }

  if (!node.text.empty()) {
    append_text_commands(
        document, node_id, node.layout.checkbox.label_rect, node, context,
        resolved, node.text, resolved.text_color, 0.0f, false, false
    );
  }
}

void append_slider_commands(
    UIDocument &document, UINodeId node_id, const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
) {
  const UiRect track = node.layout.slider.track_rect;
  const UiRect fill = node.layout.slider.fill_rect;
  const UiRect thumb = node.layout.slider.thumb_rect;

  auto append_rect = [&](const UiRect &rect, glm::vec4 color, float radius,
                         glm::vec4 border_color = glm::vec4(0.0f),
                         float border_width = 0.0f) {
    if (rect.width <= 0.0f || rect.height <= 0.0f || color.a <= 0.0f) {
      return;
    }

    UiDrawCommand command;
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
      track, glm::vec4(0.12f, 0.17f, 0.25f, 0.92f), track.height * 0.5f
  );
  append_rect(fill, node.style.accent_color, track.height * 0.5f);

  glm::vec4 thumb_color = node.style.accent_color;
  if (node.paint_state.pressed) {
    thumb_color = glm::mix(thumb_color, glm::vec4(1.0f), 0.2f);
  } else if (node.paint_state.hovered || node.paint_state.focused) {
    thumb_color = glm::mix(thumb_color, glm::vec4(1.0f), 0.1f);
  }

  append_rect(
      thumb, thumb_color, thumb.width * 0.5f,
      glm::vec4(0.02f, 0.05f, 0.1f, 0.9f), 1.0f
  );
}

void append_select_field_commands(
    UIDocument &document, UINodeId node_id, const UIDocument::UINode &node,
    const UILayoutContext &context, const UIResolvedStyle &resolved
) {
  const UiRect content = node.layout.content_bounds;
  const float indicator = std::max(12.0f, node.style.control_indicator_size);
  const float line_height = measure_line_height(node, context);
  const bool has_selection =
      !node.select.options.empty() &&
      node.select.selected_index < node.select.options.size();
  const std::string_view label = select_display_text(node);

  const UiRect text_rect = resolve_single_line_text_rect(
      UiRect{
          .x = content.x,
          .y = content.y,
          .width = std::max(
              0.0f, content.width - indicator - node.style.control_gap
          ),
          .height = content.height,
      },
      line_height
  );
  append_text_commands(
      document, node_id, text_rect, node, context, resolved, label,
      has_selection ? resolved.text_color : resolved.placeholder_text_color,
      0.0f, false, false
  );

  UiDrawCommand arrow_command;
  arrow_command.type = DrawCommandType::Text;
  arrow_command.node_id = node_id;
  arrow_command.rect = UiRect{
      .x = std::max(content.x, content.right() - indicator),
      .y = content.y,
      .width = indicator,
      .height = content.height,
  };
  apply_content_clip(arrow_command, node);
  arrow_command.text_origin = glm::vec2(arrow_command.rect.x, text_rect.y);
  arrow_command.text = node.select.open ? "^" : "v";
  arrow_command.font_id = resolve_font_id(node, context);
  arrow_command.font_size = resolve_font_size(node, context);
  arrow_command.color = resolved.text_color;
  arrow_command.color.a *= resolved.opacity;
  document.draw_list().commands.push_back(std::move(arrow_command));
}

void append_select_overlay_commands(
    UIDocument &document, UINodeId node_id, const UILayoutContext &context
) {
  const auto *node = document.node(node_id);
  if (node == nullptr || node->type != NodeType::Select || !node->visible ||
      !node->enabled || !node->select.open ||
      node->layout.select.popup_rect.width <= 0.0f ||
      node->layout.select.popup_rect.height <= 0.0f) {
    return;
  }

  const UIResolvedStyle resolved =
      resolve_style(node->style, node->paint_state, true);

  UiDrawCommand popup_command;
  popup_command.type = DrawCommandType::Rect;
  popup_command.node_id = node_id;
  popup_command.rect = node->layout.select.popup_rect;
  popup_command.color = glm::vec4(0.02f, 0.05f, 0.1f, 0.96f) *
                        glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
  popup_command.border_color =
      resolved.border_color * glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
  popup_command.border_width = std::max(1.0f, resolved.border_width);
  popup_command.border_radius = std::max(8.0f, resolved.border_radius);
  document.draw_list().commands.push_back(std::move(popup_command));

  const float font_size = resolve_font_size(*node, context);
  const float line_height = measure_line_height(*node, context);
  for (size_t index = 0; index < node->layout.select.option_rects.size();
       ++index) {
    const UiRect option_rect = node->layout.select.option_rects[index];
    const bool selected = index == node->select.selected_index;
    const bool hovered = node->layout.select.hovered_option_index.has_value() &&
                         *node->layout.select.hovered_option_index == index;
    const bool highlighted = index == node->select.highlighted_index;

    if (selected || hovered || highlighted) {
      UiDrawCommand option_bg;
      option_bg.type = DrawCommandType::Rect;
      option_bg.node_id = node_id;
      option_bg.rect = option_rect;
      option_bg.color = node->style.accent_color * glm::vec4(
                                                       1.0f, 1.0f, 1.0f,
                                                       (hovered       ? 0.28f
                                                        : highlighted ? 0.22f
                                                                      : 0.18f) *
                                                           resolved.opacity
                                                   );
      option_bg.border_radius = 6.0f;
      document.draw_list().commands.push_back(std::move(option_bg));
    }

    UiDrawCommand option_text;
    option_text.type = DrawCommandType::Text;
    option_text.node_id = node_id;
    option_text.rect = option_rect;
    option_text.text_origin = glm::vec2(
        option_rect.x + node->style.padding.left,
        option_rect.y +
            std::max(0.0f, (option_rect.height - line_height) * 0.5f)
    );
    option_text.text = node->select.options[index];
    option_text.font_id = resolve_font_id(*node, context);
    option_text.font_size = font_size;
    option_text.color = resolved.text_color;
    option_text.color.a *= resolved.opacity;
    document.draw_list().commands.push_back(std::move(option_text));
  }
}

void append_segmented_control_commands(
    UIDocument &document, UINodeId node_id, const UIDocument::UINode &node,
    const UILayoutContext &context, const UIResolvedStyle &resolved
) {
  const float line_height = measure_line_height(node, context);
  for (size_t index = 0u; index < node.layout.segmented_control.item_rects.size();
       ++index) {
    const UiRect item_rect = node.layout.segmented_control.item_rects[index];
    const bool selected = index == node.segmented_control.selected_index;
    const bool hovered =
        node.layout.segmented_control.hovered_item_index.has_value() &&
        *node.layout.segmented_control.hovered_item_index == index;
    const bool active = node.layout.segmented_control.active_item_index.has_value() &&
                        *node.layout.segmented_control.active_item_index == index;

    UiDrawCommand bg_command;
    bg_command.type = DrawCommandType::Rect;
    bg_command.node_id = node_id;
    bg_command.rect = item_rect;
    apply_content_clip(bg_command, node);
    bg_command.color =
        (selected
             ? node.style.accent_color *
                   glm::vec4(1.0f, 1.0f, 1.0f, active ? 0.4f : 0.28f)
         : hovered ? glm::vec4(0.16f, 0.24f, 0.35f, active ? 0.95f : 0.82f)
         : active  ? glm::vec4(0.1f, 0.16f, 0.24f, 0.98f)
                   : glm::vec4(0.0f));
    bg_command.color *= glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    bg_command.border_color =
        (selected ? node.style.accent_color
                  : glm::vec4(0.32f, 0.45f, 0.6f, hovered ? 0.5f : 0.28f)) *
        glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    bg_command.border_width = 1.0f;
    bg_command.border_radius = item_rect.height * 0.5f;
    document.draw_list().commands.push_back(std::move(bg_command));

    UiDrawCommand text_command;
    text_command.type = DrawCommandType::Text;
    text_command.node_id = node_id;
    text_command.rect = item_rect;
    apply_content_clip(text_command, node);
    text_command.text_origin = glm::vec2(
        item_rect.x + k_segmented_item_padding_x,
        item_rect.y + std::max(0.0f, (item_rect.height - line_height) * 0.5f)
    );
    text_command.text = node.segmented_control.options[index];
    text_command.font_id = resolve_font_id(node, context);
    text_command.font_size = resolve_font_size(node, context);
    text_command.color = selected
                             ? glm::vec4(0.98f, 1.0f, 1.0f, 1.0f)
                             : resolved.text_color;
    text_command.color.a *= resolved.opacity;
    document.draw_list().commands.push_back(std::move(text_command));
  }
}

void append_chip_group_commands(
    UIDocument &document, UINodeId node_id, const UIDocument::UINode &node,
    const UILayoutContext &context, const UIResolvedStyle &resolved
) {
  const float line_height = measure_line_height(node, context);
  for (size_t index = 0u; index < node.layout.chip_group.item_rects.size();
       ++index) {
    const UiRect item_rect = node.layout.chip_group.item_rects[index];
    const bool selected =
        index < node.chip_group.selected.size() && node.chip_group.selected[index];
    const bool hovered = node.layout.chip_group.hovered_item_index.has_value() &&
                         *node.layout.chip_group.hovered_item_index == index;
    const bool active = node.layout.chip_group.active_item_index.has_value() &&
                        *node.layout.chip_group.active_item_index == index;

    UiDrawCommand bg_command;
    bg_command.type = DrawCommandType::Rect;
    bg_command.node_id = node_id;
    bg_command.rect = item_rect;
    apply_content_clip(bg_command, node);
    bg_command.color =
        (selected
             ? node.style.accent_color *
                   glm::vec4(1.0f, 1.0f, 1.0f, active ? 0.32f : 0.22f)
         : hovered ? glm::vec4(0.12f, 0.18f, 0.28f, active ? 0.96f : 0.82f)
         : active  ? glm::vec4(0.08f, 0.13f, 0.21f, 0.96f)
                   : glm::vec4(0.04f, 0.08f, 0.13f, 0.74f));
    bg_command.color *= glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    bg_command.border_color =
        (selected ? node.style.accent_color
                  : glm::vec4(0.3f, 0.43f, 0.58f, hovered ? 0.42f : 0.24f)) *
        glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    bg_command.border_width = 1.0f;
    bg_command.border_radius = item_rect.height * 0.5f;
    document.draw_list().commands.push_back(std::move(bg_command));

    UiDrawCommand text_command;
    text_command.type = DrawCommandType::Text;
    text_command.node_id = node_id;
    text_command.rect = item_rect;
    apply_content_clip(text_command, node);
    text_command.text_origin = glm::vec2(
        item_rect.x + k_chip_item_padding_x,
        item_rect.y + std::max(0.0f, (item_rect.height - line_height) * 0.5f)
    );
    text_command.text = node.chip_group.options[index];
    text_command.font_id = resolve_font_id(node, context);
    text_command.font_size = resolve_font_size(node, context);
    text_command.color = selected
                             ? glm::vec4(0.97f, 1.0f, 1.0f, 1.0f)
                             : resolved.text_color;
    text_command.color.a *= resolved.opacity;
    document.draw_list().commands.push_back(std::move(text_command));
  }
}

void append_scrollbar_commands(
    UIDocument &document, UINodeId node_id, const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
) {
  const float thickness = std::max(0.0f, node.style.scrollbar_thickness);
  if (node.type != NodeType::ScrollView || thickness <= 0.0f) {
    return;
  }

  auto append_rect = [&](const UiRect &rect, glm::vec4 color) {
    if (rect.width <= 0.0f || rect.height <= 0.0f || color.a <= 0.0f) {
      return;
    }

    UiDrawCommand command;
    command.type = DrawCommandType::Rect;
    command.node_id = node_id;
    command.rect = rect;
    apply_content_clip(command, node);
    command.color = color * glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    command.border_radius = thickness * 0.5f;
    document.draw_list().commands.push_back(std::move(command));
  };

  if (node.layout.scroll.vertical_scrollbar_visible) {
    append_rect(
        node.layout.scroll.vertical_track_rect, node.style.scrollbar_track_color
    );
    append_rect(
        node.layout.scroll.vertical_thumb_rect,
        node.layout.scroll.vertical_thumb_active
            ? node.style.scrollbar_thumb_active_color
        : node.layout.scroll.vertical_thumb_hovered
            ? node.style.scrollbar_thumb_hovered_color
            : node.style.scrollbar_thumb_color
    );
  }

  if (node.layout.scroll.horizontal_scrollbar_visible) {
    append_rect(
        node.layout.scroll.horizontal_track_rect,
        node.style.scrollbar_track_color
    );
    append_rect(
        node.layout.scroll.horizontal_thumb_rect,
        node.layout.scroll.horizontal_thumb_active
            ? node.style.scrollbar_thumb_active_color
        : node.layout.scroll.horizontal_thumb_hovered
            ? node.style.scrollbar_thumb_hovered_color
            : node.style.scrollbar_thumb_color
    );
  }
}

void append_resize_handle_commands(
    UIDocument &document, UINodeId node_id, const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
) {
  if (!node_supports_panel_resize(node)) {
    return;
  }

  const UIHitPart part = node.layout.resize_active_part != UIHitPart::Body
                             ? node.layout.resize_active_part
                             : node.layout.resize_hovered_part;
  if (!is_panel_resize_part(part)) {
    return;
  }

  UiDrawCommand command;
  command.type = DrawCommandType::Rect;
  command.node_id = node_id;
  command.rect = resize_handle_rect(node, part);
  apply_self_clip(command, node);
  command.color = (node.layout.resize_active_part == part
                       ? node.style.resize_handle_active_color
                       : node.style.resize_handle_hovered_color) *
                  glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
  command.border_radius = is_corner_resize_part(part) ? 4.0f : 2.0f;
  document.draw_list().commands.push_back(std::move(command));
}

glm::vec2 measure_intrinsic_size(
    const UIDocument &document, UINodeId node_id, glm::vec2 available_size,
    const UILayoutContext &context
);

glm::vec2 measure_container_size(
    const UIDocument &document, UINodeId node_id, glm::vec2 available_size,
    const UILayoutContext &context
) {
  const auto *node = document.node(node_id);
  if (node == nullptr) {
    return glm::vec2(0.0f);
  }

  const UiRect available_rect{
      .width = std::max(0.0f, available_size.x),
      .height = std::max(0.0f, available_size.y),
  };
  const UiRect inner_rect = inset_rect(available_rect, node->style.padding);

  float total_main = 0.0f;
  float max_cross = 0.0f;
  uint32_t flow_children = 0u;

  for (UINodeId child_id : node->children) {
    const auto *child = document.node(child_id);
    if (child == nullptr || !child->visible ||
        child->style.position_type == PositionType::Absolute) {
      continue;
    }

    const glm::vec2 child_size = measure_intrinsic_size(
        document, child_id, glm::vec2(inner_rect.width, inner_rect.height),
        context
    );

    const float horizontal_margin = child->style.margin.horizontal();
    const float vertical_margin = child->style.margin.vertical();
    if (node->style.flex_direction == FlexDirection::Row) {
      total_main += child_size.x + horizontal_margin;
      max_cross = std::max(max_cross, child_size.y + vertical_margin);
    } else {
      total_main += child_size.y + vertical_margin;
      max_cross = std::max(max_cross, child_size.x + horizontal_margin);
    }

    flow_children++;
  }

  if (flow_children > 1u) {
    total_main += node->style.gap * static_cast<float>(flow_children - 1u);
  }

  if (node->style.flex_direction == FlexDirection::Row) {
    return glm::vec2(
        total_main + node->style.padding.horizontal(),
        max_cross + node->style.padding.vertical()
    );
  }

  return glm::vec2(
      max_cross + node->style.padding.horizontal(),
      total_main + node->style.padding.vertical()
  );
}

glm::vec2 measure_intrinsic_size(
    const UIDocument &document, UINodeId node_id, glm::vec2 available_size,
    const UILayoutContext &context
) {
  const auto *node = document.node(node_id);
  if (node == nullptr || !node->visible) {
    return glm::vec2(0.0f);
  }

  float width = 0.0f;
  float height = 0.0f;

  switch (node->type) {
    case NodeType::Text: {
      auto font = resolve_font(*node, context);
      const float font_size = resolve_font_size(*node, context);
      if (font != nullptr) {
        const glm::vec2 text_size = font->measure_text(node->text, font_size);
        width = text_size.x;
        height = text_size.y;
      }
      break;
    }
    case NodeType::Image: {
      auto texture =
          resource_manager()->get_by_descriptor_id<Texture2D>(node->texture_id);
      if (texture != nullptr) {
        width = static_cast<float>(texture->width());
        height = static_cast<float>(texture->height());
      }
      break;
    }
    case NodeType::View:
    case NodeType::Pressable:
    case NodeType::ScrollView:
      if (!node->children.empty()) {
        const glm::vec2 container_size =
            measure_container_size(document, node_id, available_size, context);
        width = container_size.x;
        height = container_size.y;
      } else {
        width = node->style.padding.horizontal();
        height = node->style.padding.vertical();
      }
      break;
    case NodeType::TextInput: {
      const glm::vec2 input_size = measure_text_input_size(*node, context);
      width = input_size.x;
      height = input_size.y;
      break;
    }
    case NodeType::Splitter: {
      const auto *parent = document.node(node->parent);
      const bool row_parent =
          parent == nullptr ||
          parent->style.flex_direction == FlexDirection::Row;
      const float thickness = std::max(1.0f, node->style.splitter_thickness);
      width = row_parent ? thickness : 0.0f;
      height = row_parent ? 0.0f : thickness;
      break;
    }
    case NodeType::Checkbox: {
      const glm::vec2 checkbox_size = measure_checkbox_size(*node, context);
      width = checkbox_size.x;
      height = checkbox_size.y;
      break;
    }
    case NodeType::Slider: {
      const glm::vec2 slider_size = measure_slider_size(*node, context);
      width = slider_size.x;
      height = slider_size.y;
      break;
    }
    case NodeType::Select: {
      const glm::vec2 select_size = measure_select_size(*node, context);
      width = select_size.x;
      height = select_size.y;
      break;
    }
    case NodeType::SegmentedControl: {
      const glm::vec2 segmented_size =
          measure_segmented_control_size(*node, context);
      width = segmented_size.x;
      height = segmented_size.y;
      break;
    }
    case NodeType::ChipGroup: {
      const glm::vec2 chip_group_size = measure_chip_group_size(*node, context);
      width = chip_group_size.x;
      height = chip_group_size.y;
      break;
    }
  }

  width = resolve_length(
      node->style.width, available_size.x, context.default_font_size, width
  );
  height = resolve_length(
      node->style.height, available_size.y, context.default_font_size, height
  );

  width = clamp_dimension(
      width, node->style.min_width, node->style.max_width, available_size.x,
      context.default_font_size
  );
  height = clamp_dimension(
      height, node->style.min_height, node->style.max_height, available_size.y,
      context.default_font_size
  );

  return glm::vec2(width, height);
}

struct FlowItem {
  UINodeId node_id = k_invalid_node_id;
  glm::vec2 preferred_size = glm::vec2(0.0f);
  float main_size = 0.0f;
  float cross_size = 0.0f;
  float main_margin_leading = 0.0f;
  float main_margin_trailing = 0.0f;
  float cross_margin_leading = 0.0f;
  float cross_margin_trailing = 0.0f;
  float flex_grow = 0.0f;
  float flex_shrink = 0.0f;
  AlignItems align = AlignItems::Stretch;
};

AlignItems resolve_align(const UIDocument::UINode &node, AlignItems parent) {
  switch (node.style.align_self) {
    case AlignSelf::Start:
      return AlignItems::Start;
    case AlignSelf::Center:
      return AlignItems::Center;
    case AlignSelf::End:
      return AlignItems::End;
    case AlignSelf::Stretch:
      return AlignItems::Stretch;
    case AlignSelf::Auto:
    default:
      return parent;
  }
}

void layout_node(
    UIDocument &document, UINodeId node_id, const UiRect &bounds,
    std::optional<UiRect> inherited_clip, const UILayoutContext &context
);

void layout_children(
    UIDocument &document, UINodeId node_id, const UILayoutContext &context
) {
  auto *node = document.node(node_id);
  if (node == nullptr) {
    return;
  }

  const UiRect inner_bounds = node->layout.content_bounds;
  node->layout.scroll.viewport_size =
      glm::vec2(inner_bounds.width, inner_bounds.height);
  std::vector<FlowItem> flow_items;

  for (UINodeId child_id : node->children) {
    auto *child = document.node(child_id);
    if (child == nullptr || !child->visible ||
        child->style.position_type == PositionType::Absolute) {
      continue;
    }

    const glm::vec2 preferred = measure_intrinsic_size(
        document, child_id, glm::vec2(inner_bounds.width, inner_bounds.height),
        context
    );

    FlowItem item;
    item.node_id = child_id;
    item.preferred_size = preferred;
    item.flex_grow = child->style.flex_grow;
    item.flex_shrink = child->style.flex_shrink;
    item.align = resolve_align(*child, node->style.align_items);

    if (node->style.flex_direction == FlexDirection::Row) {
      item.main_size = resolve_length(
          child->style.flex_basis, inner_bounds.width,
          context.default_font_size,
          resolve_length(
              child->style.width, inner_bounds.width, context.default_font_size,
              preferred.x
          )
      );
      item.cross_size = resolve_length(
          child->style.height, inner_bounds.height, context.default_font_size,
          preferred.y
      );
      item.main_margin_leading = child->style.margin.left;
      item.main_margin_trailing = child->style.margin.right;
      item.cross_margin_leading = child->style.margin.top;
      item.cross_margin_trailing = child->style.margin.bottom;
    } else {
      item.main_size = resolve_length(
          child->style.flex_basis, inner_bounds.height,
          context.default_font_size,
          resolve_length(
              child->style.height, inner_bounds.height,
              context.default_font_size, preferred.y
          )
      );
      item.cross_size = resolve_length(
          child->style.width, inner_bounds.width, context.default_font_size,
          preferred.x
      );
      item.main_margin_leading = child->style.margin.top;
      item.main_margin_trailing = child->style.margin.bottom;
      item.cross_margin_leading = child->style.margin.left;
      item.cross_margin_trailing = child->style.margin.right;
    }

    flow_items.push_back(item);
  }

  const float container_main = node->style.flex_direction == FlexDirection::Row
                                   ? inner_bounds.width
                                   : inner_bounds.height;
  const float container_cross = node->style.flex_direction == FlexDirection::Row
                                    ? inner_bounds.height
                                    : inner_bounds.width;

  float total_main = 0.0f;
  float total_flex_grow = 0.0f;
  float total_flex_shrink = 0.0f;

  for (const FlowItem &item : flow_items) {
    total_main +=
        item.main_size + item.main_margin_leading + item.main_margin_trailing;
    total_flex_grow += item.flex_grow;
    total_flex_shrink += item.flex_shrink * item.main_size;
  }

  if (flow_items.size() > 1u) {
    total_main += node->style.gap * static_cast<float>(flow_items.size() - 1u);
  }

  const float free_space = container_main - total_main;
  const bool scrolls_along_main_axis =
      node->type == NodeType::ScrollView &&
      ((node->style.flex_direction == FlexDirection::Row &&
        scrolls_horizontally(node->style.scroll_mode)) ||
       (node->style.flex_direction == FlexDirection::Column &&
        scrolls_vertically(node->style.scroll_mode)) ||
       node->style.scroll_mode == ScrollMode::Both);

  if (!scrolls_along_main_axis) {
    if (free_space > 0.0f && total_flex_grow > 0.0f) {
      for (FlowItem &item : flow_items) {
        item.main_size += free_space * (item.flex_grow / total_flex_grow);
      }
    } else if (free_space < 0.0f && total_flex_shrink > 0.0f) {
      for (FlowItem &item : flow_items) {
        const float shrink_basis =
            (item.flex_shrink * item.main_size) / total_flex_shrink;
        item.main_size =
            std::max(0.0f, item.main_size + free_space * shrink_basis);
      }
    }
  }

  float consumed_main = 0.0f;
  for (const FlowItem &item : flow_items) {
    consumed_main +=
        item.main_size + item.main_margin_leading + item.main_margin_trailing;
  }

  if (flow_items.size() > 1u) {
    consumed_main +=
        node->style.gap * static_cast<float>(flow_items.size() - 1u);
  }

  float leading_space = 0.0f;
  float between_space = node->style.gap;
  const float remaining_space = std::max(0.0f, container_main - consumed_main);

  switch (node->style.justify_content) {
    case JustifyContent::Center:
      leading_space = remaining_space * 0.5f;
      break;
    case JustifyContent::End:
      leading_space = remaining_space;
      break;
    case JustifyContent::SpaceBetween:
      between_space = flow_items.size() > 1u
                          ? remaining_space / (flow_items.size() - 1u)
                          : 0.0f;
      break;
    case JustifyContent::SpaceAround:
      between_space =
          flow_items.empty() ? 0.0f : remaining_space / flow_items.size();
      leading_space = between_space * 0.5f;
      break;
    case JustifyContent::SpaceEvenly:
      between_space = flow_items.empty()
                          ? 0.0f
                          : remaining_space / (flow_items.size() + 1u);
      leading_space = between_space;
      break;
    case JustifyContent::Start:
    default:
      break;
  }

  std::vector<std::pair<UINodeId, UiRect>> planned_bounds;
  planned_bounds.reserve(node->children.size());

  float cursor = leading_space;
  float content_width = 0.0f;
  float content_height = 0.0f;
  for (const FlowItem &item : flow_items) {
    auto *child = document.node(item.node_id);
    if (child == nullptr) {
      continue;
    }

    float cross_size = item.cross_size;
    if (item.align == AlignItems::Stretch &&
        ((node->style.flex_direction == FlexDirection::Row &&
          child->style.height.unit == UiLengthUnit::Auto) ||
         (node->style.flex_direction == FlexDirection::Column &&
          child->style.width.unit == UiLengthUnit::Auto))) {
      cross_size = std::max(
          0.0f, container_cross - item.cross_margin_leading -
                    item.cross_margin_trailing
      );
    }

    float cross_offset = 0.0f;
    switch (item.align) {
      case AlignItems::Center:
        cross_offset =
            (container_cross - cross_size - item.cross_margin_leading -
             item.cross_margin_trailing) *
            0.5f;
        break;
      case AlignItems::End:
        cross_offset = container_cross - cross_size -
                       item.cross_margin_leading - item.cross_margin_trailing;
        break;
      case AlignItems::Stretch:
      case AlignItems::Start:
      default:
        cross_offset = 0.0f;
        break;
    }

    UiRect child_bounds;
    if (node->style.flex_direction == FlexDirection::Row) {
      child_bounds = UiRect{
          .x = inner_bounds.x + cursor + item.main_margin_leading,
          .y = inner_bounds.y + cross_offset + item.cross_margin_leading,
          .width = item.main_size,
          .height = cross_size,
      };
    } else {
      child_bounds = UiRect{
          .x = inner_bounds.x + cross_offset + item.cross_margin_leading,
          .y = inner_bounds.y + cursor + item.main_margin_leading,
          .width = cross_size,
          .height = item.main_size,
      };
    }

    planned_bounds.emplace_back(item.node_id, child_bounds);
    content_width =
        std::max(content_width, child_bounds.right() - inner_bounds.x);
    content_height =
        std::max(content_height, child_bounds.bottom() - inner_bounds.y);

    cursor += item.main_margin_leading + item.main_size +
              item.main_margin_trailing + between_space;
  }

  for (UINodeId child_id : node->children) {
    auto *child = document.node(child_id);
    if (child == nullptr || !child->visible ||
        child->style.position_type != PositionType::Absolute) {
      continue;
    }

    const glm::vec2 preferred = measure_intrinsic_size(
        document, child_id, glm::vec2(inner_bounds.width, inner_bounds.height),
        context
    );

    float width =
        resolve_length(
            child->style.width, inner_bounds.width, context.default_font_size,
            preferred.x
        );
    float height =
        resolve_length(
            child->style.height, inner_bounds.height, context.default_font_size,
            preferred.y
        );

    if (child->style.left.unit != UiLengthUnit::Auto &&
        child->style.right.unit != UiLengthUnit::Auto &&
        child->style.width.unit == UiLengthUnit::Auto) {
      width = std::max(
          0.0f, inner_bounds.width -
                    resolve_length(
                        child->style.left, inner_bounds.width,
                        context.default_font_size
                    ) -
                    resolve_length(
                        child->style.right, inner_bounds.width,
                        context.default_font_size
                    )
      );
    }

    if (child->style.top.unit != UiLengthUnit::Auto &&
        child->style.bottom.unit != UiLengthUnit::Auto &&
        child->style.height.unit == UiLengthUnit::Auto) {
      height = std::max(
          0.0f, inner_bounds.height -
                    resolve_length(
                        child->style.top, inner_bounds.height,
                        context.default_font_size
                    ) -
                    resolve_length(
                        child->style.bottom, inner_bounds.height,
                        context.default_font_size
                    )
      );
    }

    const float x =
        child->style.left.unit != UiLengthUnit::Auto
            ? inner_bounds.x +
                  resolve_length(
                      child->style.left, inner_bounds.width,
                      context.default_font_size
                  )
            : inner_bounds.right() -
                  resolve_length(
                      child->style.right, inner_bounds.width,
                      context.default_font_size
                  ) -
                  width;
    const float y =
        child->style.top.unit != UiLengthUnit::Auto
            ? inner_bounds.y +
                  resolve_length(
                      child->style.top, inner_bounds.height,
                      context.default_font_size
                  )
            : inner_bounds.bottom() -
                  resolve_length(
                      child->style.bottom, inner_bounds.height,
                      context.default_font_size
                  ) -
                  height;

    UiRect child_bounds{.x = x, .y = y, .width = width, .height = height};
    if (node_supports_panel_resize(*child)) {
      child_bounds = clamp_rect_to_bounds(child_bounds, inner_bounds);
    }
    planned_bounds.emplace_back(child_id, child_bounds);
    content_width =
        std::max(content_width, child_bounds.right() - inner_bounds.x);
    content_height =
        std::max(content_height, child_bounds.bottom() - inner_bounds.y);
  }

  node->layout.scroll.content_size = glm::vec2(content_width, content_height);
  node->layout.scroll.max_offset = glm::max(
      node->layout.scroll.content_size - node->layout.scroll.viewport_size,
      glm::vec2(0.0f)
  );
  node->layout.scroll.offset = clamp_scroll_offset(
      node->layout.scroll.offset, node->layout.scroll.max_offset,
      node->style.scroll_mode
  );
  update_scrollbar_geometry(*node);

  const glm::vec2 scroll_translation(
      scrolls_horizontally(node->style.scroll_mode)
          ? -node->layout.scroll.offset.x
          : 0.0f,
      scrolls_vertically(node->style.scroll_mode)
          ? -node->layout.scroll.offset.y
          : 0.0f
  );

  for (const auto &[child_id, planned_bounds_rect] : planned_bounds) {
    UiRect translated_bounds = planned_bounds_rect;
    translated_bounds.x += scroll_translation.x;
    translated_bounds.y += scroll_translation.y;

    layout_node(
        document, child_id, translated_bounds, child_clip_rect(*node), context
    );
  }
}

void layout_node(
    UIDocument &document, UINodeId node_id, const UiRect &bounds,
    std::optional<UiRect> inherited_clip, const UILayoutContext &context
) {
  auto *node = document.node(node_id);
  if (node == nullptr) {
    return;
  }

  node->layout.bounds = bounds;
  node->layout.content_bounds = inset_rect(bounds, node->style.padding);
  node->layout.measured_size = glm::vec2(bounds.width, bounds.height);

  node->layout.has_clip = inherited_clip.has_value();
  node->layout.clip_bounds = inherited_clip.value_or(UiRect{});

  std::optional<UiRect> child_clip = inherited_clip;
  if (node->style.overflow == Overflow::Hidden) {
    child_clip = child_clip.has_value()
                     ? intersect_rect(*child_clip, node->layout.content_bounds)
                     : node->layout.content_bounds;
  }

  node->layout.has_content_clip = child_clip.has_value();
  node->layout.content_clip_bounds = child_clip.value_or(UiRect{});

  layout_children(document, node_id, context);

  switch (node->type) {
    case NodeType::Checkbox:
      update_checkbox_layout(*node, context);
      break;
    case NodeType::Slider:
      update_slider_layout(*node);
      break;
    case NodeType::Select:
      update_select_layout(*node, context);
      break;
    case NodeType::SegmentedControl:
      update_segmented_control_layout(*node, context);
      break;
    case NodeType::ChipGroup:
      update_chip_group_layout(*node, context);
      break;
    default:
      break;
  }
}

void append_draw_commands(
    UIDocument &document, UINodeId node_id, const UILayoutContext &context,
    bool parent_enabled = true
) {
  auto *node = document.node(node_id);
  if (node == nullptr || !node->visible) {
    return;
  }

  const bool effective_enabled = parent_enabled && node->enabled;
  const UIResolvedStyle resolved =
      resolve_style(node->style, node->paint_state, effective_enabled);
  const UiRect bounds = node->layout.bounds;
  const UiRect content_bounds = node->layout.content_bounds;

  if ((resolved.background_color.a > 0.0f || resolved.border_width > 0.0f) &&
      bounds.width > 0.0f && bounds.height > 0.0f) {
    glm::vec4 fill = resolved.background_color;
    fill.a *= resolved.opacity;
    UiDrawCommand command;
    command.type = DrawCommandType::Rect;
    command.node_id = node_id;
    command.rect = bounds;
    apply_self_clip(command, *node);
    command.color = fill;
    command.border_color = resolved.border_color;
    command.border_color.a *= resolved.opacity;
    command.border_width = resolved.border_width;
    command.border_radius = resolved.border_radius;
    document.draw_list().commands.push_back(std::move(command));
  }

  if (node->type == NodeType::Image && !node->texture_id.empty() &&
      content_bounds.width > 0.0f && content_bounds.height > 0.0f) {
    UiDrawCommand command;
    command.type = DrawCommandType::Image;
    command.node_id = node_id;
    command.rect = content_bounds;
    apply_content_clip(command, *node);
    command.texture_id = node->texture_id;
    command.tint =
        resolved.tint * glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    document.draw_list().commands.push_back(std::move(command));
  }

  if (node->type == NodeType::Text && !node->text.empty()) {
    append_text_commands(
        document, node_id, bounds, *node, context, resolved, node->text,
        resolved.text_color, 0.0f, !node->selection.empty(),
        node->paint_state.focused && node->caret.active
    );
  }

  if (node->type == NodeType::TextInput) {
    auto font = resolve_font(*node, context);
    const float font_size = resolve_font_size(*node, context);
    const float line_height =
        font != nullptr ? font->line_height(font_size) : font_size;
    const UiRect text_rect =
        resolve_single_line_text_rect(content_bounds, line_height);

    if (!node->text.empty()) {
      append_text_commands(
          document, node_id, text_rect, *node, context, resolved, node->text,
          resolved.text_color, node->text_scroll_x,
          node->paint_state.focused && !node->selection.empty(),
          node->paint_state.focused && node->caret.active
      );
    } else if (!node->placeholder.empty()) {
      UiDrawCommand placeholder_command;
      placeholder_command.type = DrawCommandType::Text;
      placeholder_command.node_id = node_id;
      placeholder_command.rect = text_rect;
      apply_content_clip(placeholder_command, *node);
      placeholder_command.text_origin = glm::vec2(text_rect.x, text_rect.y);
      placeholder_command.text = node->placeholder;
      placeholder_command.font_id = resolve_font_id(*node, context);
      placeholder_command.font_size = font_size;
      placeholder_command.color = resolved.placeholder_text_color;
      placeholder_command.color.a *= resolved.opacity;
      document.draw_list().commands.push_back(std::move(placeholder_command));

      if (node->paint_state.focused && node->caret.active &&
          node->caret.visible) {
        UiDrawCommand caret_command;
        caret_command.type = DrawCommandType::Rect;
        caret_command.node_id = node_id;
        caret_command.rect = UiRect{
            .x = text_rect.x,
            .y = text_rect.y,
            .width = 1.0f,
            .height = std::max(text_rect.height, line_height),
        };
        apply_content_clip(caret_command, *node);
        caret_command.color = resolved.caret_color;
        document.draw_list().commands.push_back(std::move(caret_command));
      }
    } else if (node->paint_state.focused && node->caret.active &&
               node->caret.visible) {
      UiDrawCommand caret_command;
      caret_command.type = DrawCommandType::Rect;
      caret_command.node_id = node_id;
      caret_command.rect = UiRect{
          .x = text_rect.x,
          .y = text_rect.y,
          .width = 1.0f,
          .height = std::max(text_rect.height, line_height),
      };
      apply_content_clip(caret_command, *node);
      caret_command.color = resolved.caret_color;
      document.draw_list().commands.push_back(std::move(caret_command));
    }
  }

  if (node->type == NodeType::Checkbox) {
    append_checkbox_commands(document, node_id, *node, context, resolved);
  }

  if (node->type == NodeType::Slider) {
    append_slider_commands(document, node_id, *node, resolved);
  }

  if (node->type == NodeType::Select) {
    append_select_field_commands(document, node_id, *node, context, resolved);
  }

  if (node->type == NodeType::SegmentedControl) {
    append_segmented_control_commands(document, node_id, *node, context,
                                      resolved);
  }

  if (node->type == NodeType::ChipGroup) {
    append_chip_group_commands(document, node_id, *node, context, resolved);
  }

  for (UINodeId child_id : node->children) {
    append_draw_commands(document, child_id, context, effective_enabled);
  }

  append_scrollbar_commands(document, node_id, *node, resolved);
  append_resize_handle_commands(document, node_id, *node, resolved);
}

std::optional<UiHitResult>
hit_test_node(const UIDocument &document, UINodeId node_id, glm::vec2 point) {
  const auto *node = document.node(node_id);
  if (node == nullptr || !node->visible || !node->enabled) {
    return std::nullopt;
  }

  if (node->layout.has_clip && !node->layout.clip_bounds.contains(point)) {
    return std::nullopt;
  }

  if (!node->layout.bounds.contains(point)) {
    return std::nullopt;
  }

  if (auto resize_hit = hit_test_resize_handles(*node, point);
      resize_hit.has_value()) {
    return UiHitResult{.node_id = node_id, .part = *resize_hit};
  }

  if (node->type == NodeType::Splitter) {
    return UiHitResult{.node_id = node_id, .part = UIHitPart::SplitterBar};
  }

  if (node->type == NodeType::Slider) {
    if (node->layout.slider.thumb_rect.contains(point)) {
      return UiHitResult{.node_id = node_id, .part = UIHitPart::SliderThumb};
    }

    return UiHitResult{.node_id = node_id, .part = UIHitPart::SliderTrack};
  }

  if (node->type == NodeType::SegmentedControl) {
    for (size_t index = 0u; index < node->layout.segmented_control.item_rects.size();
         ++index) {
      if (node->layout.segmented_control.item_rects[index].contains(point)) {
        return UiHitResult{.node_id = node_id,
                           .part = UIHitPart::SegmentedControlItem,
                           .item_index = index};
      }
    }
  }

  if (node->type == NodeType::ChipGroup) {
    for (size_t index = 0u; index < node->layout.chip_group.item_rects.size();
         ++index) {
      if (node->layout.chip_group.item_rects[index].contains(point)) {
        return UiHitResult{
            .node_id = node_id, .part = UIHitPart::ChipItem, .item_index = index};
      }
    }
  }

  if (node->type == NodeType::Select) {
    return UiHitResult{.node_id = node_id, .part = UIHitPart::SelectField};
  }

  if (node->type == NodeType::ScrollView) {
    if (node->layout.scroll.vertical_scrollbar_visible) {
      if (node->layout.scroll.vertical_thumb_rect.contains(point)) {
        return UiHitResult{.node_id = node_id,
                           .part = UIHitPart::VerticalScrollbarThumb};
      }
      if (node->layout.scroll.vertical_track_rect.contains(point)) {
        return UiHitResult{.node_id = node_id,
                           .part = UIHitPart::VerticalScrollbarTrack};
      }
    }

    if (node->layout.scroll.horizontal_scrollbar_visible) {
      if (node->layout.scroll.horizontal_thumb_rect.contains(point)) {
        return UiHitResult{.node_id = node_id,
                           .part = UIHitPart::HorizontalScrollbarThumb};
      }
      if (node->layout.scroll.horizontal_track_rect.contains(point)) {
        return UiHitResult{.node_id = node_id,
                           .part = UIHitPart::HorizontalScrollbarTrack};
      }
    }
  }

  for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
    auto hit = hit_test_node(document, *it, point);
    if (hit.has_value()) {
      return hit;
    }
  }

  return UiHitResult{
      .node_id = node_id,
      .part = node->type == NodeType::TextInput ? UIHitPart::TextInputText
                                                : UIHitPart::Body,
  };
}

} // namespace

void layout_document(UIDocument &document, const UILayoutContext &context) {
  document.set_canvas_size(context.viewport_size);
  document.set_root_font_size(context.default_font_size);

  const UINodeId root_id = document.root();
  auto *root = document.node(root_id);
  if (root == nullptr) {
    document.draw_list().clear();
    document.clear_dirty();
    return;
  }

  const glm::vec2 preferred =
      measure_intrinsic_size(document, root_id, context.viewport_size, context);

  const float width = resolve_length(
      root->style.width, context.viewport_size.x, context.default_font_size,
      std::max(context.viewport_size.x, preferred.x)
  );
  const float height = resolve_length(
      root->style.height, context.viewport_size.y, context.default_font_size,
      std::max(context.viewport_size.y, preferred.y)
  );

  layout_node(
      document, root_id,
      UiRect{
          .x = 0.0f,
          .y = 0.0f,
          .width = width,
          .height = height,
      },
      std::nullopt, context
  );

  document.mark_paint_dirty();
  document.clear_layout_dirty();
}

std::optional<UiHitResult>
hit_test_document(const UIDocument &document, glm::vec2 point) {
  if (document.root() == k_invalid_node_id) {
    return std::nullopt;
  }

  const UINodeId open_select_id = document.open_select_node();
  if (open_select_id != k_invalid_node_id) {
    if (const auto *select = document.node(open_select_id);
        select != nullptr && select->type == NodeType::Select &&
        select->visible && select->enabled && select->select.open) {
      for (size_t index = 0; index < select->layout.select.option_rects.size();
           ++index) {
        if (select->layout.select.option_rects[index].contains(point)) {
          return UiHitResult{
              .node_id = open_select_id,
              .part = UIHitPart::SelectOption,
              .item_index = index,
          };
        }
      }
    }
  }

  return hit_test_node(document, document.root(), point);
}

void build_draw_list(UIDocument &document, const UILayoutContext &context) {
  document.draw_list().clear();

  if (document.root() == k_invalid_node_id) {
    document.clear_dirty();
    return;
  }

  append_draw_commands(document, document.root(), context);
  if (document.open_select_node() != k_invalid_node_id) {
    append_select_overlay_commands(
        document, document.open_select_node(), context
    );
  }
  document.clear_paint_dirty();
}

} // namespace astralix::ui
