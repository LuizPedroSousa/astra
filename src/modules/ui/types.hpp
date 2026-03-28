#pragma once

#include "events/key-event.hpp"
#include "glm/glm.hpp"
#include "guid.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace astralix::ui {

using UINodeId = uint32_t;
constexpr UINodeId k_invalid_node_id = 0u;

enum class NodeType : uint8_t {
  View,
  Text,
  Image,
  Pressable,
  SegmentedControl,
  ChipGroup,
  TextInput,
  ScrollView,
  Splitter,
  Checkbox,
  Slider,
  Select,
};

enum class FlexDirection : uint8_t {
  Row,
  Column,
};

enum class JustifyContent : uint8_t {
  Start,
  Center,
  End,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly,
};

enum class AlignItems : uint8_t {
  Start,
  Center,
  End,
  Stretch,
};

enum class AlignSelf : uint8_t {
  Auto,
  Start,
  Center,
  End,
  Stretch,
};

enum class PositionType : uint8_t {
  Relative,
  Absolute,
};

enum class Overflow : uint8_t {
  Visible,
  Hidden,
};

enum class ScrollMode : uint8_t {
  None,
  Horizontal,
  Vertical,
  Both,
};

enum class ScrollbarVisibility : uint8_t {
  Hidden,
  Auto,
  Always,
};

enum class ResizeMode : uint8_t {
  None,
  Horizontal,
  Vertical,
  Both,
};

constexpr uint8_t k_resize_edge_none = 0u;
constexpr uint8_t k_resize_edge_left = 1u << 0u;
constexpr uint8_t k_resize_edge_top = 1u << 1u;
constexpr uint8_t k_resize_edge_right = 1u << 2u;
constexpr uint8_t k_resize_edge_bottom = 1u << 3u;
constexpr uint8_t k_resize_edge_all = k_resize_edge_left | k_resize_edge_top |
                                      k_resize_edge_right |
                                      k_resize_edge_bottom;

enum class UIHitPart : uint8_t {
  Body,
  TextInputText,
  SegmentedControlItem,
  ChipItem,
  SliderTrack,
  SliderThumb,
  SelectField,
  SelectOption,
  ResizeLeft,
  ResizeTop,
  ResizeRight,
  ResizeBottom,
  ResizeTopLeft,
  ResizeTopRight,
  ResizeBottomLeft,
  ResizeBottomRight,
  SplitterBar,
  VerticalScrollbarThumb,
  VerticalScrollbarTrack,
  HorizontalScrollbarThumb,
  HorizontalScrollbarTrack,
};

enum class UiLengthUnit : uint8_t {
  Auto,
  Pixels,
  Percent,
  Rem,
};

struct UILength {
  UiLengthUnit unit = UiLengthUnit::Auto;
  float value = 0.0f;

  static UILength pixels(float value) {
    return UILength{.unit = UiLengthUnit::Pixels, .value = value};
  }

  static UILength percent(float value) {
    return UILength{.unit = UiLengthUnit::Percent, .value = value};
  }

  static UILength rem(float value) {
    return UILength{.unit = UiLengthUnit::Rem, .value = value};
  }

  static UILength auto_value() { return UILength{}; }
};

struct UIEdges {
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;

  static UIEdges all(float value) {
    return UIEdges{
        .left = value,
        .top = value,
        .right = value,
        .bottom = value,
    };
  }

  static UIEdges symmetric(float horizontal, float vertical) {
    return UIEdges{
        .left = horizontal,
        .top = vertical,
        .right = horizontal,
        .bottom = vertical,
    };
  }

  float horizontal() const { return left + right; }
  float vertical() const { return top + bottom; }
};

struct UiRect {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;

  float right() const { return x + width; }
  float bottom() const { return y + height; }

  bool contains(glm::vec2 point) const {
    return point.x >= x && point.x <= right() && point.y >= y &&
           point.y <= bottom();
  }
};

inline bool intersects(const UiRect &lhs, const UiRect &rhs) {
  return lhs.x < rhs.right() && lhs.right() > rhs.x && lhs.y < rhs.bottom() &&
         lhs.bottom() > rhs.y;
}

inline UiRect intersect_rect(const UiRect &lhs, const UiRect &rhs) {
  const float left = std::max(lhs.x, rhs.x);
  const float top = std::max(lhs.y, rhs.y);
  const float right = std::min(lhs.right(), rhs.right());
  const float bottom = std::min(lhs.bottom(), rhs.bottom());

  if (right <= left || bottom <= top) {
    return UiRect{};
  }

  return UiRect{
      .x = left,
      .y = top,
      .width = right - left,
      .height = bottom - top,
  };
}

inline UiRect inset_rect(const UiRect &rect, const UIEdges &inset) {
  const float width = std::max(0.0f, rect.width - inset.horizontal());
  const float height = std::max(0.0f, rect.height - inset.vertical());

  return UiRect{
      .x = rect.x + inset.left,
      .y = rect.y + inset.top,
      .width = width,
      .height = height,
  };
}

struct UiStateStyle {
  std::optional<glm::vec4> background_color;
  std::optional<glm::vec4> border_color;
  std::optional<float> border_width;
  std::optional<float> border_radius;
  std::optional<float> opacity;
  std::optional<glm::vec4> tint;
  std::optional<glm::vec4> text_color;
};

struct UIStyle {
  FlexDirection flex_direction = FlexDirection::Column;
  JustifyContent justify_content = JustifyContent::Start;
  AlignItems align_items = AlignItems::Stretch;
  AlignSelf align_self = AlignSelf::Auto;
  PositionType position_type = PositionType::Relative;
  Overflow overflow = Overflow::Visible;
  ScrollMode scroll_mode = ScrollMode::None;
  ScrollbarVisibility scrollbar_visibility = ScrollbarVisibility::Hidden;
  ResizeMode resize_mode = ResizeMode::None;
  uint8_t resize_edges = k_resize_edge_none;
  bool draggable = false;
  bool drag_handle = false;

  float flex_grow = 0.0f;
  float flex_shrink = 0.0f;
  UILength flex_basis = UILength::auto_value();

  UILength width = UILength::auto_value();
  UILength height = UILength::auto_value();
  UILength min_width = UILength::auto_value();
  UILength min_height = UILength::auto_value();
  UILength max_width = UILength::auto_value();
  UILength max_height = UILength::auto_value();

  UILength left = UILength::auto_value();
  UILength top = UILength::auto_value();
  UILength right = UILength::auto_value();
  UILength bottom = UILength::auto_value();

  UIEdges margin;
  UIEdges padding;
  float gap = 0.0f;

  glm::vec4 background_color = glm::vec4(0.0f);
  glm::vec4 border_color = glm::vec4(0.0f);
  float border_width = 0.0f;
  float border_radius = 0.0f;
  float opacity = 1.0f;
  glm::vec4 tint = glm::vec4(1.0f);

  glm::vec4 text_color = glm::vec4(1.0f);
  ResourceDescriptorID font_id;
  float font_size = 0.0f;
  float line_height = 0.0f;
  glm::vec4 placeholder_text_color = glm::vec4(0.62f, 0.69f, 0.78f, 0.95f);
  glm::vec4 selection_color = glm::vec4(0.34f, 0.56f, 0.95f, 0.35f);
  glm::vec4 caret_color = glm::vec4(0.95f, 0.97f, 1.0f, 1.0f);
  float scrollbar_thickness = 10.0f;
  float scrollbar_min_thumb_length = 24.0f;
  glm::vec4 scrollbar_track_color = glm::vec4(0.12f, 0.17f, 0.24f, 0.55f);
  glm::vec4 scrollbar_thumb_color = glm::vec4(0.42f, 0.55f, 0.71f, 0.72f);
  glm::vec4 scrollbar_thumb_hovered_color =
      glm::vec4(0.55f, 0.69f, 0.86f, 0.88f);
  glm::vec4 scrollbar_thumb_active_color =
      glm::vec4(0.66f, 0.79f, 0.96f, 0.96f);
  float resize_handle_thickness = 6.0f;
  float resize_corner_extent = 16.0f;
  glm::vec4 resize_handle_color = glm::vec4(0.56f, 0.69f, 0.86f, 0.22f);
  glm::vec4 resize_handle_hovered_color = glm::vec4(0.68f, 0.82f, 1.0f, 0.55f);
  glm::vec4 resize_handle_active_color = glm::vec4(0.78f, 0.9f, 1.0f, 0.78f);
  float splitter_thickness = 6.0f;
  glm::vec4 accent_color = glm::vec4(0.56f, 0.72f, 0.96f, 1.0f);
  float control_gap = 8.0f;
  float control_indicator_size = 16.0f;
  float slider_track_thickness = 6.0f;
  float slider_thumb_radius = 8.0f;

  UiStateStyle hovered_style;
  UiStateStyle pressed_style;
  UiStateStyle focused_style;
  UiStateStyle disabled_style;
};

struct UiCheckboxState {
  bool checked = false;
};

struct UiSliderState {
  float value = 0.0f;
  float min_value = 0.0f;
  float max_value = 1.0f;
  float step = 0.1f;
};

struct UiSelectState {
  std::vector<std::string> options;
  size_t selected_index = 0u;
  size_t highlighted_index = 0u;
  bool open = false;
};

struct UiSegmentedControlState {
  std::vector<std::string> options;
  size_t selected_index = 0u;
};

struct UiChipGroupState {
  std::vector<std::string> options;
  std::vector<bool> selected;
};

struct UiScrollState {
  glm::vec2 offset = glm::vec2(0.0f);
  glm::vec2 max_offset = glm::vec2(0.0f);
  glm::vec2 content_size = glm::vec2(0.0f);
  glm::vec2 viewport_size = glm::vec2(0.0f);
  bool vertical_scrollbar_visible = false;
  bool horizontal_scrollbar_visible = false;
  UiRect vertical_track_rect;
  UiRect vertical_thumb_rect;
  UiRect horizontal_track_rect;
  UiRect horizontal_thumb_rect;
  bool vertical_thumb_hovered = false;
  bool vertical_thumb_active = false;
  bool horizontal_thumb_hovered = false;
  bool horizontal_thumb_active = false;
};

struct UiLayoutMetrics {
  struct CheckboxLayout {
    UiRect indicator_rect;
    UiRect label_rect;
  };

  struct SliderLayout {
    UiRect track_rect;
    UiRect fill_rect;
    UiRect thumb_rect;
  };

  struct SelectLayout {
    UiRect popup_rect;
    std::vector<UiRect> option_rects;
    std::optional<size_t> hovered_option_index;
  };

  struct SegmentedControlLayout {
    std::vector<UiRect> item_rects;
    std::optional<size_t> hovered_item_index;
    std::optional<size_t> active_item_index;
  };

  struct ChipGroupLayout {
    std::vector<UiRect> item_rects;
    std::optional<size_t> hovered_item_index;
    std::optional<size_t> active_item_index;
  };

  UiRect bounds;
  UiRect content_bounds;
  UiRect clip_bounds;
  bool has_clip = false;
  UiRect content_clip_bounds;
  glm::vec2 measured_size = glm::vec2(0.0f);
  CheckboxLayout checkbox;
  SliderLayout slider;
  SelectLayout select;
  SegmentedControlLayout segmented_control;
  ChipGroupLayout chip_group;
  UiScrollState scroll;
  UIHitPart resize_hovered_part = UIHitPart::Body;
  UIHitPart resize_active_part = UIHitPart::Body;
  bool has_content_clip = false;
};

struct UIPaintState {
  bool hovered = false;
  bool pressed = false;
  bool focused = false;
};

struct UITextSelection {
  size_t anchor = 0u;
  size_t focus = 0u;

  bool empty() const { return anchor == focus; }
  size_t start() const { return std::min(anchor, focus); }
  size_t end() const { return std::max(anchor, focus); }
};

struct UiCaretState {
  size_t index = 0u;
  bool active = false;
  bool visible = true;
  double blink_elapsed = 0.0;
};

struct UIKeyInputEvent {
  input::KeyCode key_code = input::KeyCode::Space;
  input::KeyModifiers modifiers;
  bool repeat = false;
};

struct UiCharacterInputEvent {
  uint32_t codepoint = 0u;
  input::KeyModifiers modifiers;
};

struct UiMouseWheelInputEvent {
  glm::vec2 offset = glm::vec2(0.0f);
  input::KeyModifiers modifiers;
};

struct UiHitResult {
  UINodeId node_id = k_invalid_node_id;
  UIHitPart part = UIHitPart::Body;
  std::optional<size_t> item_index;
};

struct UIResolvedStyle {
  glm::vec4 background_color = glm::vec4(0.0f);
  glm::vec4 border_color = glm::vec4(0.0f);
  float border_width = 0.0f;
  float border_radius = 0.0f;
  float opacity = 1.0f;
  glm::vec4 tint = glm::vec4(1.0f);
  glm::vec4 text_color = glm::vec4(1.0f);
  glm::vec4 placeholder_text_color = glm::vec4(0.62f, 0.69f, 0.78f, 0.95f);
  glm::vec4 selection_color = glm::vec4(0.34f, 0.56f, 0.95f, 0.35f);
  glm::vec4 caret_color = glm::vec4(0.95f, 0.97f, 1.0f, 1.0f);
};

enum class DrawCommandType : uint8_t {
  Rect,
  Image,
  Text,
};

struct UiDrawCommand {
  DrawCommandType type = DrawCommandType::Rect;
  UINodeId node_id = k_invalid_node_id;
  UiRect rect;
  UiRect clip_rect;
  bool has_clip = false;
  glm::vec4 color = glm::vec4(1.0f);
  glm::vec4 border_color = glm::vec4(0.0f);
  float border_width = 0.0f;
  float border_radius = 0.0f;
  glm::vec2 text_origin = glm::vec2(0.0f);
  std::string text;
  ResourceDescriptorID font_id;
  float font_size = 0.0f;
  ResourceDescriptorID texture_id;
  glm::vec4 tint = glm::vec4(1.0f);
};

struct UiDrawList {
  std::vector<UiDrawCommand> commands;

  void clear() { commands.clear(); }
};

struct UILayoutContext {
  glm::vec2 viewport_size = glm::vec2(0.0f);
  ResourceDescriptorID default_font_id;
  float default_font_size = 16.0f;
};

} // namespace astralix::ui
