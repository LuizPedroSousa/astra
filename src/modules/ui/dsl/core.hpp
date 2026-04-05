#pragma once

#include "assert.hpp"
#include "containers/vector.hpp"
#include "document/document.hpp"
#include "types.hpp"

#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace astralix::ui::dsl {

enum class StyleTarget : uint8_t {
  Base,
  Hovered,
  Pressed,
  Focused,
  Disabled,
};

enum class StyleOpCode : uint8_t {
  FlexDirection,
  JustifyContent,
  AlignItems,
  AlignSelf,
  Gap,
  FlexGrow,
  FlexShrink,
  FlexBasis,
  Width,
  Height,
  MaxWidth,
  MaxHeight,
  MinWidth,
  MinHeight,
  Left,
  Top,
  Right,
  Bottom,
  PositionType,
  Padding,
  PaddingXY,
  PaddingY,
  PaddingX,
  PaddingBottom,
  BackgroundColor,
  Border,
  BorderRadius,
  Opacity,
  Tint,
  TextColor,
  FontSize,
  PlaceholderTextColor,
  SelectionColor,
  CaretColor,
  Overflow,
  ScrollMode,
  ScrollbarVisibility,
  ScrollbarThickness,
  ScrollbarTrackColor,
  ScrollbarThumbColor,
  ScrollbarThumbHoveredColor,
  ScrollbarThumbActiveColor,
  ResizeMode,
  Draggable,
  DragHandle,
  ResizeHandleThickness,
  ResizeCornerExtent,
  AccentColor,
  ControlGap,
  ControlIndicatorSize,
  SliderTrackThickness,
  SliderThumbRadius,
  Cursor,
};

struct StyleBorderValue {
  float width = 0.0f;
  glm::vec4 color = glm::vec4(0.0f);
};

struct StyleFloatPair {
  float first = 0.0f;
  float second = 0.0f;
};

struct StyleResizeValue {
  ResizeMode mode = ResizeMode::None;
  uint8_t edges = k_resize_edge_none;
};

union StyleOpPayload {
  std::monostate none;
  float float_value;
  glm::vec4 vec4_value;
  UILength length_value;
  UIEdges edges_value;
  FlexDirection flex_direction_value;
  JustifyContent justify_content_value;
  AlignItems align_items_value;
  AlignSelf align_self_value;
  PositionType position_type_value;
  Overflow overflow_value;
  ScrollMode scroll_mode_value;
  ScrollbarVisibility scrollbar_visibility_value;
  StyleBorderValue border_value;
  StyleFloatPair float_pair_value;
  StyleResizeValue resize_value;
  CursorStyle cursor_value;

  StyleOpPayload() : none{} {}
  StyleOpPayload(std::monostate value) : none(value) {}
  StyleOpPayload(float value) : float_value(value) {}
  StyleOpPayload(glm::vec4 value) : vec4_value(value) {}
  StyleOpPayload(UILength value) : length_value(value) {}
  StyleOpPayload(UIEdges value) : edges_value(value) {}
  StyleOpPayload(FlexDirection value) : flex_direction_value(value) {}
  StyleOpPayload(JustifyContent value) : justify_content_value(value) {}
  StyleOpPayload(AlignItems value) : align_items_value(value) {}
  StyleOpPayload(AlignSelf value) : align_self_value(value) {}
  StyleOpPayload(PositionType value) : position_type_value(value) {}
  StyleOpPayload(Overflow value) : overflow_value(value) {}
  StyleOpPayload(ScrollMode value) : scroll_mode_value(value) {}
  StyleOpPayload(ScrollbarVisibility value)
      : scrollbar_visibility_value(value) {}
  StyleOpPayload(StyleBorderValue value) : border_value(value) {}
  StyleOpPayload(StyleFloatPair value) : float_pair_value(value) {}
  StyleOpPayload(StyleResizeValue value) : resize_value(value) {}
  StyleOpPayload(CursorStyle value) : cursor_value(value) {}

  StyleOpPayload(const StyleOpPayload &) = default;
  StyleOpPayload(StyleOpPayload &&) = default;
  StyleOpPayload &operator=(const StyleOpPayload &) = default;
  StyleOpPayload &operator=(StyleOpPayload &&) = default;
};

struct StateStyleOp {
  StyleOpCode code = StyleOpCode::BackgroundColor;
  StyleOpPayload payload;

  StateStyleOp() = default;
  explicit StateStyleOp(StyleOpCode next_code) : code(next_code) {}
  StateStyleOp(StyleOpCode next_code, StyleOpPayload next_payload)
      : code(next_code), payload(std::move(next_payload)) {}

  template <typename T>
    requires(
                !std::is_same_v<std::remove_cvref_t<T>, StyleOpPayload> &&
                std::is_constructible_v<StyleOpPayload, T>
            )
  StateStyleOp(StyleOpCode next_code, T next_payload)
      : code(next_code), payload(std::move(next_payload)) {}
};

struct StyleOp {
  StyleTarget target = StyleTarget::Base;
  StyleOpCode code = StyleOpCode::BackgroundColor;
  StyleOpPayload payload;

  StyleOp() = default;
  StyleOp(StyleTarget next_target, StyleOpCode next_code)
      : target(next_target), code(next_code) {}
  StyleOp(
      StyleTarget next_target,
      StyleOpCode next_code,
      StyleOpPayload next_payload
  )
      : target(next_target), code(next_code),
        payload(std::move(next_payload)) {}

  template <typename T>
    requires(
                !std::is_same_v<std::remove_cvref_t<T>, StyleOpPayload> &&
                std::is_constructible_v<StyleOpPayload, T>
            )
  StyleOp(StyleTarget next_target, StyleOpCode next_code, T next_payload)
      : target(next_target), code(next_code),
        payload(std::move(next_payload)) {}
};

struct FontIdStyleStep {
  ResourceDescriptorID value;
};

using StateStyleStep = StateStyleOp;
using StyleStep = std::variant<StyleOp, FontIdStyleStep>;

template <typename StepContainer>
void reserve_style_steps(StepContainer &steps, size_t capacity) {
  if (steps.capacity() < capacity) {
    steps.reserve(capacity);
  }
}

using StateStyleStepStorage = SmallVector<StateStyleStep, 4>;
using StyleStepStorage = SmallVector<StyleStep, 8>;

inline UIStateStyle &resolve_style_target(
    UIStyle &style,
    StyleTarget target
) {
  switch (target) {
    case StyleTarget::Hovered:
      return style.hovered_style;
    case StyleTarget::Pressed:
      return style.pressed_style;
    case StyleTarget::Focused:
      return style.focused_style;
    case StyleTarget::Disabled:
      return style.disabled_style;
    case StyleTarget::Base:
      break;
  }

  ASTRA_ENSURE(
      true, "ui::dsl base style target cannot resolve to a UIStateStyle"
  );
  return style.hovered_style;
}

inline void apply_state_style_op(UIStateStyle &style, const StateStyleOp &op) {
  switch (op.code) {
    case StyleOpCode::BackgroundColor:
      style.background_color = op.payload.vec4_value;
      return;
    case StyleOpCode::Border: {
      const auto &border = op.payload.border_value;
      style.border_width = border.width;
      style.border_color = border.color;
      return;
    }
    case StyleOpCode::BorderRadius:
      style.border_radius = op.payload.float_value;
      return;
    case StyleOpCode::Opacity:
      style.opacity = op.payload.float_value;
      return;
    case StyleOpCode::Tint:
      style.tint = op.payload.vec4_value;
      return;
    case StyleOpCode::TextColor:
      style.text_color = op.payload.vec4_value;
      return;
    default:
      break;
  }

  ASTRA_ENSURE(true, "ui::dsl encountered unsupported state style op");
}

inline void apply_style_op(UIStyle &style, const StyleOp &op) {
  UIStateStyle *state_target = nullptr;
  if (op.target != StyleTarget::Base) {
    state_target = &resolve_style_target(style, op.target);
  }

  switch (op.code) {
    case StyleOpCode::BackgroundColor:
      if (state_target != nullptr) {
        state_target->background_color = op.payload.vec4_value;
      } else {
        style.background_color = op.payload.vec4_value;
      }
      return;
    case StyleOpCode::Border:
      if (state_target != nullptr) {
        const auto &border = op.payload.border_value;
        state_target->border_width = border.width;
        state_target->border_color = border.color;
      } else {
        const auto &border = op.payload.border_value;
        style.border_width = border.width;
        style.border_color = border.color;
      }
      return;
    case StyleOpCode::BorderRadius:
      if (state_target != nullptr) {
        state_target->border_radius = op.payload.float_value;
      } else {
        style.border_radius = op.payload.float_value;
      }
      return;
    case StyleOpCode::Opacity:
      ASTRA_ENSURE(
          state_target == nullptr,
          "ui::dsl opacity state op is only valid on state styles"
      );
      state_target->opacity = op.payload.float_value;
      return;
    case StyleOpCode::Tint:
      if (state_target != nullptr) {
        state_target->tint = op.payload.vec4_value;
      } else {
        style.tint = op.payload.vec4_value;
      }
      return;
    case StyleOpCode::TextColor:
      if (state_target != nullptr) {
        state_target->text_color = op.payload.vec4_value;
      } else {
        style.text_color = op.payload.vec4_value;
      }
      return;
    default:
      break;
  }

  ASTRA_ENSURE(
      state_target != nullptr,
      "ui::dsl base-only style op was applied to a state style"
  );

  switch (op.code) {
    case StyleOpCode::FlexDirection:
      style.flex_direction = op.payload.flex_direction_value;
      return;
    case StyleOpCode::JustifyContent:
      style.justify_content = op.payload.justify_content_value;
      return;
    case StyleOpCode::AlignItems:
      style.align_items = op.payload.align_items_value;
      return;
    case StyleOpCode::AlignSelf:
      style.align_self = op.payload.align_self_value;
      return;
    case StyleOpCode::Gap:
      style.gap = op.payload.float_value;
      return;
    case StyleOpCode::FlexGrow:
      style.flex_grow = op.payload.float_value;
      return;
    case StyleOpCode::FlexShrink:
      style.flex_shrink = op.payload.float_value;
      return;
    case StyleOpCode::FlexBasis:
      style.flex_basis = op.payload.length_value;
      return;
    case StyleOpCode::Width:
      style.width = op.payload.length_value;
      return;
    case StyleOpCode::Height:
      style.height = op.payload.length_value;
      return;
    case StyleOpCode::MaxWidth:
      style.max_width = op.payload.length_value;
      return;
    case StyleOpCode::MaxHeight:
      style.max_height = op.payload.length_value;
      return;
    case StyleOpCode::MinWidth:
      style.min_width = op.payload.length_value;
      return;
    case StyleOpCode::MinHeight:
      style.min_height = op.payload.length_value;
      return;
    case StyleOpCode::Left:
      style.left = op.payload.length_value;
      return;
    case StyleOpCode::Top:
      style.top = op.payload.length_value;
      return;
    case StyleOpCode::Right:
      style.right = op.payload.length_value;
      return;
    case StyleOpCode::Bottom:
      style.bottom = op.payload.length_value;
      return;
    case StyleOpCode::PositionType:
      style.position_type = op.payload.position_type_value;
      return;
    case StyleOpCode::Padding:
      style.padding = op.payload.edges_value;
      return;
    case StyleOpCode::PaddingXY: {
      const auto &pair = op.payload.float_pair_value;
      style.padding = UIEdges::symmetric(pair.first, pair.second);
      return;
    }
    case StyleOpCode::PaddingY:
      style.padding = UIEdges::symmetric(
          style.padding.horizontal(), op.payload.float_value
      );
      return;
    case StyleOpCode::PaddingX:
      style.padding = UIEdges::symmetric(
          op.payload.float_value, style.padding.vertical()
      );
      return;
    case StyleOpCode::PaddingBottom:
      style.padding = UIEdges{.bottom = op.payload.float_value};
      return;
    case StyleOpCode::FontSize:
      style.font_size = op.payload.float_value;
      return;
    case StyleOpCode::PlaceholderTextColor:
      style.placeholder_text_color = op.payload.vec4_value;
      return;
    case StyleOpCode::SelectionColor:
      style.selection_color = op.payload.vec4_value;
      return;
    case StyleOpCode::CaretColor:
      style.caret_color = op.payload.vec4_value;
      return;
    case StyleOpCode::Overflow:
      style.overflow = op.payload.overflow_value;
      return;
    case StyleOpCode::ScrollMode:
      style.scroll_mode = op.payload.scroll_mode_value;
      return;
    case StyleOpCode::ScrollbarVisibility:
      style.scrollbar_visibility = op.payload.scrollbar_visibility_value;
      return;
    case StyleOpCode::ScrollbarThickness:
      style.scrollbar_thickness = op.payload.float_value;
      return;
    case StyleOpCode::ScrollbarTrackColor:
      style.scrollbar_track_color = op.payload.vec4_value;
      return;
    case StyleOpCode::ScrollbarThumbColor:
      style.scrollbar_thumb_color = op.payload.vec4_value;
      return;
    case StyleOpCode::ScrollbarThumbHoveredColor:
      style.scrollbar_thumb_hovered_color = op.payload.vec4_value;
      return;
    case StyleOpCode::ScrollbarThumbActiveColor:
      style.scrollbar_thumb_active_color = op.payload.vec4_value;
      return;
    case StyleOpCode::ResizeMode: {
      const auto &resize = op.payload.resize_value;
      style.resize_mode = resize.mode;
      style.resize_edges = resize.edges;
      return;
    }
    case StyleOpCode::Draggable:
      style.draggable = true;
      return;
    case StyleOpCode::DragHandle:
      style.drag_handle = true;
      return;
    case StyleOpCode::ResizeHandleThickness:
      style.resize_handle_thickness = op.payload.float_value;
      return;
    case StyleOpCode::ResizeCornerExtent:
      style.resize_corner_extent = op.payload.float_value;
      return;
    case StyleOpCode::AccentColor:
      style.accent_color = op.payload.vec4_value;
      return;
    case StyleOpCode::ControlGap:
      style.control_gap = op.payload.float_value;
      return;
    case StyleOpCode::ControlIndicatorSize:
      style.control_indicator_size = op.payload.float_value;
      return;
    case StyleOpCode::SliderTrackThickness:
      style.slider_track_thickness = op.payload.float_value;
      return;
    case StyleOpCode::SliderThumbRadius:
      style.slider_thumb_radius = op.payload.float_value;
      return;
    case StyleOpCode::Cursor:
      style.cursor = op.payload.cursor_value;
      return;
    default:
      break;
  }

  ASTRA_ENSURE(true, "ui::dsl encountered unsupported base style op");
}

inline void apply_style_step(UIStyle &style, const StyleStep &step) {
  if (auto *op = std::get_if<StyleOp>(&step)) {
    apply_style_op(style, *op);
    return;
  }

  if (const auto *font_id = std::get_if<FontIdStyleStep>(&step);
      font_id != nullptr) {
    style.font_id = font_id->value;
    return;
  }
}

class StateStyleBuilder {
public:
  explicit operator bool() const { return !m_steps.empty(); }
  bool empty() const { return m_steps.empty(); }
  size_t step_count() const { return m_steps.size(); }

  void append_steps_to(StateStyleStepStorage &steps) const & {
    reserve_style_steps(steps, steps.size() + m_steps.size());
    steps.append_copy(m_steps);
  }

  void append_steps_to(StateStyleStepStorage &steps) && {
    reserve_style_steps(steps, steps.size() + m_steps.size());
    steps.append_move(m_steps);
  }

  StateStyleBuilder &background(glm::vec4 color) {
    return add(StateStyleOp{StyleOpCode::BackgroundColor, color});
  }

  StateStyleBuilder &border(float width, glm::vec4 color) {
    return add(StateStyleOp{
        StyleOpCode::Border,
        StyleBorderValue{.width = width, .color = color},
    });
  }

  StateStyleBuilder &radius(float value) {
    return add(StateStyleOp{StyleOpCode::BorderRadius, value});
  }

  StateStyleBuilder &opacity(float value) {
    return add(StateStyleOp{StyleOpCode::Opacity, value});
  }

  StateStyleBuilder &tint(glm::vec4 value) {
    return add(StateStyleOp{StyleOpCode::Tint, value});
  }

  StateStyleBuilder &text_color(glm::vec4 color) {
    return add(StateStyleOp{StyleOpCode::TextColor, color});
  }

private:
  friend class StyleBuilder;

  StateStyleBuilder &add(StateStyleOp op) {
    reserve_style_steps(m_steps, 4u);
    m_steps.emplace_back(std::move(op));
    return *this;
  }

  StateStyleBuilder &add(StateStyleBuilder builder) {
    if (!builder.empty()) {
      m_steps.append_move(builder.m_steps);
    }
    return *this;
  }

  StateStyleStepStorage m_steps;
};

class StyleBuilder {
public:
  explicit operator bool() const { return !m_steps.empty(); }
  bool empty() const { return m_steps.empty(); }
  size_t step_count() const { return m_steps.size(); }

  void append_steps_to(StyleStepStorage &steps) const & {
    reserve_style_steps(steps, steps.size() + m_steps.size());
    steps.append_copy(m_steps);
  }

  void append_steps_to(StyleStepStorage &steps) && {
    reserve_style_steps(steps, steps.size() + m_steps.size());
    steps.append_move(m_steps);
  }

  StyleBuilder &flex_row() {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::FlexDirection, FlexDirection::Row});
  }

  StyleBuilder &column() {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::FlexDirection, FlexDirection::Column});
  }

  StyleBuilder &items_start() {
    return add(
        StyleOp{StyleTarget::Base, StyleOpCode::AlignItems, AlignItems::Start}
    );
  }

  StyleBuilder &items_end() {
    return add(
        StyleOp{StyleTarget::Base, StyleOpCode::AlignItems, AlignItems::End}
    );
  }

  StyleBuilder &items_center() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::AlignItems,
        AlignItems::Center,
    });
  }

  StyleBuilder &align_self(AlignSelf value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::AlignSelf, value});
  }

  StyleBuilder &self_end() { return align_self(AlignSelf::End); }

  StyleBuilder &justify_start() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::JustifyContent,
        JustifyContent::Start,
    });
  }

  StyleBuilder &justify_end() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::JustifyContent,
        JustifyContent::End,
    });
  }

  StyleBuilder &justify_center() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::JustifyContent,
        JustifyContent::Center,
    });
  }

  StyleBuilder &justify_between() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::JustifyContent,
        JustifyContent::SpaceBetween,
    });
  }

  StyleBuilder &gap(float value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::Gap, value});
  }

  StyleBuilder &grow(float value = 1.0f) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::FlexGrow, value});
  }

  StyleBuilder &shrink(float value = 1.0f) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::FlexShrink, value});
  }

  StyleBuilder &basis(UILength value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::FlexBasis, value});
  }

  StyleBuilder &flex(float value = 1.0f) {
    add(StyleOp{StyleTarget::Base, StyleOpCode::FlexGrow, value});
    add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::FlexShrink,
        value > 0.0f ? 1.0f : 0.0f,
    });
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::FlexBasis,
        value > 0.0f ? UILength::pixels(0.0f) : UILength::auto_value(),
    });
  }

  StyleBuilder &width(UILength value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::Width, value});
  }

  StyleBuilder &height(UILength value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::Height, value});
  }

  StyleBuilder &max_width(UILength value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::MaxWidth, value});
  }

  StyleBuilder &max_height(UILength value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::MaxHeight, value});
  }

  StyleBuilder &min_width(UILength value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::MinWidth, value});
  }

  StyleBuilder &min_height(UILength value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::MinHeight, value});
  }

  StyleBuilder &left(UILength value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::Left, value});
  }

  StyleBuilder &top(UILength value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::Top, value});
  }

  StyleBuilder &right(UILength value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::Right, value});
  }

  StyleBuilder &bottom(UILength value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::Bottom, value});
  }

  StyleBuilder &fill_x() { return width(UILength::percent(1.0f)); }

  StyleBuilder &fill_y() { return height(UILength::percent(1.0f)); }

  StyleBuilder &fill() { return fill_x().fill_y(); }

  StyleBuilder &absolute() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::PositionType,
        PositionType::Absolute,
    });
  }

  StyleBuilder &relative() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::PositionType,
        PositionType::Relative,
    });
  }

  StyleBuilder &padding(float value) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::Padding,
        UIEdges::all(value),
    });
  }

  StyleBuilder &padding(UIEdges value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::Padding, value});
  }

  StyleBuilder &padding_xy(float horizontal, float vertical) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::PaddingXY,
        StyleFloatPair{.first = horizontal, .second = vertical},
    });
  }

  StyleBuilder &padding_y(float vertical) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::PaddingY, vertical});
  }

  StyleBuilder &padding_x(float horizontal) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::PaddingX, horizontal});
  }

  StyleBuilder &padding_bottom(float value) {
    return add(
        StyleOp{StyleTarget::Base, StyleOpCode::PaddingBottom, value}
    );
  }

  StyleBuilder &background(glm::vec4 color) {
    return add(
        StyleOp{StyleTarget::Base, StyleOpCode::BackgroundColor, color}
    );
  }

  StyleBuilder &border(float width, glm::vec4 color) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::Border,
        StyleBorderValue{.width = width, .color = color},
    });
  }

  StyleBuilder &radius(float value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::BorderRadius, value});
  }

  StyleBuilder &text_color(glm::vec4 color) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::TextColor, color});
  }

  StyleBuilder &tint(glm::vec4 color) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::Tint, color});
  }

  StyleBuilder &font_id(ResourceDescriptorID value) {
    return add_font_id(std::move(value));
  }

  StyleBuilder &font_size(float value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::FontSize, value});
  }

  StyleBuilder &placeholder_text_color(glm::vec4 color) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::PlaceholderTextColor,
        color,
    });
  }

  StyleBuilder &selection_color(glm::vec4 color) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::SelectionColor,
        color,
    });
  }

  StyleBuilder &caret_color(glm::vec4 color) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::CaretColor,
        color,
    });
  }

  StyleBuilder &overflow_hidden() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::Overflow,
        Overflow::Hidden,
    });
  }

  StyleBuilder &scroll_both() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ScrollMode,
        ScrollMode::Both,
    });
  }

  StyleBuilder &scroll_vertical() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ScrollMode,
        ScrollMode::Vertical,
    });
  }

  StyleBuilder &scroll_horizontal() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ScrollMode,
        ScrollMode::Horizontal,
    });
  }

  StyleBuilder &scrollbar_auto() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ScrollbarVisibility,
        ScrollbarVisibility::Auto,
    });
  }

  StyleBuilder &scrollbar_thickness(float value) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ScrollbarThickness,
        value,
    });
  }

  StyleBuilder &scrollbar_track_color(glm::vec4 color) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ScrollbarTrackColor,
        color,
    });
  }

  StyleBuilder &scrollbar_thumb_color(glm::vec4 color) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ScrollbarThumbColor,
        color,
    });
  }

  StyleBuilder &scrollbar_thumb_hovered_color(glm::vec4 color) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ScrollbarThumbHoveredColor,
        color,
    });
  }

  StyleBuilder &scrollbar_thumb_active_color(glm::vec4 color) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ScrollbarThumbActiveColor,
        color,
    });
  }

  StyleBuilder &resizable_all() {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ResizeMode,
        StyleResizeValue{
            .mode = ResizeMode::Both,
            .edges = k_resize_edge_all,
        },
    });
  }

  StyleBuilder &draggable() {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::Draggable});
  }

  StyleBuilder &drag_handle() {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::DragHandle});
  }

  StyleBuilder &handle(float value) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ResizeHandleThickness,
        value,
    });
  }

  StyleBuilder &corner(float value) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ResizeCornerExtent,
        value,
    });
  }

  StyleBuilder &accent_color(glm::vec4 color) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::AccentColor, color});
  }

  StyleBuilder &control_gap(float value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::ControlGap, value});
  }

  StyleBuilder &control_indicator_size(float value) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::ControlIndicatorSize,
        value,
    });
  }

  StyleBuilder &slider_track_thickness(float value) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::SliderTrackThickness,
        value,
    });
  }

  StyleBuilder &slider_thumb_radius(float value) {
    return add(StyleOp{
        StyleTarget::Base,
        StyleOpCode::SliderThumbRadius,
        value,
    });
  }

  StyleBuilder &cursor(CursorStyle value) {
    return add(StyleOp{StyleTarget::Base, StyleOpCode::Cursor, value});
  }

  StyleBuilder &cursor_pointer() { return cursor(CursorStyle::Pointer); }

  StyleBuilder &hover(StateStyleBuilder builder) {
    return add(StyleTarget::Hovered, std::move(builder));
  }

  StyleBuilder &pressed(StateStyleBuilder builder) {
    return add(StyleTarget::Pressed, std::move(builder));
  }

  StyleBuilder &focused(StateStyleBuilder builder) {
    return add(StyleTarget::Focused, std::move(builder));
  }

  StyleBuilder &disabled(StateStyleBuilder builder) {
    return add(StyleTarget::Disabled, std::move(builder));
  }

private:
  StyleBuilder &add(StyleOp op) {
    reserve_style_steps(m_steps, 8u);
    m_steps.emplace_back(std::move(op));
    return *this;
  }

  StyleBuilder &add_font_id(ResourceDescriptorID value) {
    reserve_style_steps(m_steps, m_steps.size() + 1u);
    m_steps.emplace_back(FontIdStyleStep{.value = std::move(value)});
    return *this;
  }

  StyleBuilder &add(StyleBuilder builder) {
    if (!builder.empty()) {
      m_steps.append_move(builder.m_steps);
    }
    return *this;
  }

  StyleBuilder &add(StyleTarget target, StateStyleBuilder builder) {
    if (!builder.empty()) {
      reserve_style_steps(m_steps, m_steps.size() + builder.m_steps.size());
      for (const auto &op : builder.m_steps) {
        m_steps.emplace_back(StyleOp{target, op.code, op.payload});
      }
    }
    return *this;
  }

  StyleStepStorage m_steps;
};

namespace styles {

inline UILength px(float value) { return UILength::pixels(value); }
inline UILength percent(float value) { return UILength::percent(value); }
inline UILength rem(float value) { return UILength::rem(value); }
inline UILength max_content() { return UILength::max_content(); }

inline glm::vec4 rgba(float r, float g, float b, float a) {
  return glm::vec4(r, g, b, a);
}

inline StateStyleBuilder state() { return StateStyleBuilder{}; }
inline StyleBuilder panel() { return StyleBuilder{}; }
inline StyleBuilder absolute() { return StyleBuilder{}.absolute(); }
inline StyleBuilder relative() { return StyleBuilder{}.relative(); }
inline StyleBuilder resizable_all() { return StyleBuilder{}.resizable_all(); }
inline StyleBuilder draggable() { return StyleBuilder{}.draggable(); }
inline StyleBuilder drag_handle() { return StyleBuilder{}.drag_handle(); }
inline StyleBuilder cursor_pointer() { return StyleBuilder{}.cursor_pointer(); }
inline StyleBuilder row() { return StyleBuilder{}.flex_row(); }
inline StyleBuilder column() { return StyleBuilder{}.column(); }
inline StyleBuilder items_start() { return StyleBuilder{}.items_start(); }
inline StyleBuilder items_end() { return StyleBuilder{}.items_end(); }
inline StyleBuilder items_center() { return StyleBuilder{}.items_center(); }
inline StyleBuilder align_self(AlignSelf value) {
  return StyleBuilder{}.align_self(value);
}
inline StyleBuilder self_end() { return StyleBuilder{}.self_end(); }
inline StyleBuilder justify_start() { return StyleBuilder{}.justify_start(); }
inline StyleBuilder justify_end() { return StyleBuilder{}.justify_end(); }
inline StyleBuilder justify_center() {
  return StyleBuilder{}.justify_center();
}
inline StyleBuilder gap(float value) { return StyleBuilder{}.gap(value); }
inline StyleBuilder grow(float value = 1.0f) {
  return StyleBuilder{}.grow(value);
}
inline StyleBuilder shrink(float value = 1.0f) {
  return StyleBuilder{}.shrink(value);
}
inline StyleBuilder basis(UILength value) { return StyleBuilder{}.basis(value); }
inline StyleBuilder flex(float value = 1.0f) {
  return StyleBuilder{}.flex(value);
}
inline StyleBuilder width(UILength value) { return StyleBuilder{}.width(value); }
inline StyleBuilder height(UILength value) {
  return StyleBuilder{}.height(value);
}
inline StyleBuilder max_width(UILength value) {
  return StyleBuilder{}.max_width(value);
}
inline StyleBuilder max_height(UILength value) {
  return StyleBuilder{}.max_height(value);
}
inline StyleBuilder min_width(UILength value) {
  return StyleBuilder{}.min_width(value);
}
inline StyleBuilder min_height(UILength value) {
  return StyleBuilder{}.min_height(value);
}
inline StyleBuilder left(UILength value) { return StyleBuilder{}.left(value); }
inline StyleBuilder top(UILength value) { return StyleBuilder{}.top(value); }
inline StyleBuilder right(UILength value) { return StyleBuilder{}.right(value); }
inline StyleBuilder bottom(UILength value) {
  return StyleBuilder{}.bottom(value);
}
inline StyleBuilder fill_x() { return StyleBuilder{}.fill_x(); }
inline StyleBuilder fill_y() { return StyleBuilder{}.fill_y(); }
inline StyleBuilder fill() { return StyleBuilder{}.fill(); }
inline StyleBuilder padding(float value) {
  return StyleBuilder{}.padding(value);
}
inline StyleBuilder padding_xy(float horizontal, float vertical) {
  return StyleBuilder{}.padding_xy(horizontal, vertical);
}
inline StyleBuilder padding_y(float vertical) {
  return StyleBuilder{}.padding_y(vertical);
}
inline StyleBuilder padding_x(float horizontal) {
  return StyleBuilder{}.padding_x(horizontal);
}
inline StyleBuilder background(glm::vec4 color) {
  return StyleBuilder{}.background(color);
}
inline StyleBuilder border(float width, glm::vec4 color) {
  return StyleBuilder{}.border(width, color);
}
inline StyleBuilder radius(float value) { return StyleBuilder{}.radius(value); }
inline StyleBuilder text_color(glm::vec4 color) {
  return StyleBuilder{}.text_color(color);
}
inline StyleBuilder tint(glm::vec4 color) { return StyleBuilder{}.tint(color); }
inline StyleBuilder font_id(ResourceDescriptorID value) {
  return StyleBuilder{}.font_id(std::move(value));
}
inline StyleBuilder font_size(float value) {
  return StyleBuilder{}.font_size(value);
}
inline StyleBuilder placeholder_text_color(glm::vec4 color) {
  return StyleBuilder{}.placeholder_text_color(color);
}
inline StyleBuilder selection_color(glm::vec4 color) {
  return StyleBuilder{}.selection_color(color);
}
inline StyleBuilder caret_color(glm::vec4 color) {
  return StyleBuilder{}.caret_color(color);
}
inline StyleBuilder overflow_hidden() {
  return StyleBuilder{}.overflow_hidden();
}
inline StyleBuilder scroll_both() { return StyleBuilder{}.scroll_both(); }
inline StyleBuilder scroll_vertical() {
  return StyleBuilder{}.scroll_vertical();
}
inline StyleBuilder scroll_horizontal() {
  return StyleBuilder{}.scroll_horizontal();
}
inline StyleBuilder scrollbar_auto() {
  return StyleBuilder{}.scrollbar_auto();
}
inline StyleBuilder scrollbar_thickness(float value) {
  return StyleBuilder{}.scrollbar_thickness(value);
}
inline StyleBuilder scrollbar_track_color(glm::vec4 color) {
  return StyleBuilder{}.scrollbar_track_color(color);
}
inline StyleBuilder scrollbar_thumb_color(glm::vec4 color) {
  return StyleBuilder{}.scrollbar_thumb_color(color);
}
inline StyleBuilder scrollbar_thumb_hovered_color(glm::vec4 color) {
  return StyleBuilder{}.scrollbar_thumb_hovered_color(color);
}
inline StyleBuilder scrollbar_thumb_active_color(glm::vec4 color) {
  return StyleBuilder{}.scrollbar_thumb_active_color(color);
}
inline StyleBuilder hover(StateStyleBuilder builder) {
  return StyleBuilder{}.hover(std::move(builder));
}
inline StyleBuilder pressed(StateStyleBuilder builder) {
  return StyleBuilder{}.pressed(std::move(builder));
}
inline StyleBuilder focused(StateStyleBuilder builder) {
  return StyleBuilder{}.focused(std::move(builder));
}
inline StyleBuilder disabled(StateStyleBuilder builder) {
  return StyleBuilder{}.disabled(std::move(builder));
}
inline StyleBuilder accent_color(glm::vec4 color) {
  return StyleBuilder{}.accent_color(color);
}
inline StyleBuilder control_gap(float value) {
  return StyleBuilder{}.control_gap(value);
}
inline StyleBuilder control_indicator_size(float value) {
  return StyleBuilder{}.control_indicator_size(value);
}
inline StyleBuilder slider_track_thickness(float value) {
  return StyleBuilder{}.slider_track_thickness(value);
}
inline StyleBuilder slider_thumb_radius(float value) {
  return StyleBuilder{}.slider_thumb_radius(value);
}
} // namespace styles

enum class NodeKind : uint8_t {
  View,
  Text,
  Image,
  RenderImageView,
  Pressable,
  IconButton,
  SegmentedControl,
  ChipGroup,
  TextInput,
  Combobox,
  ScrollView,
  Popover,
  Splitter,
  Button,
  Checkbox,
  Slider,
  Select,
  LineChart,
};

struct NodeSpec {
  NodeKind kind = NodeKind::View;
  std::string text;
  std::string placeholder;
  ResourceDescriptorID texture_id;
  std::optional<RenderImageExportKey> render_image_key;
  std::vector<std::string> option_values;
  std::vector<glm::vec4> item_accent_colors;
  std::vector<bool> chip_selected_values;

  StyleStepStorage style_steps;
  std::vector<NodeSpec> child_specs;

  std::optional<bool> visible_value;
  std::optional<bool> enabled_value;
  std::optional<bool> focusable_value;
  std::optional<bool> read_only_value;
  std::optional<bool> select_all_on_focus_value;
  std::optional<bool> checked_value;
  std::optional<float> slider_value;
  std::optional<float> slider_min_value;
  std::optional<float> slider_max_value;
  std::optional<float> slider_step_value;
  std::optional<size_t> selected_index_value;

  std::optional<size_t> line_chart_grid_line_count;
  std::optional<glm::vec4> line_chart_grid_color;

  UINodeId *bound_id = nullptr;

  std::function<void()> on_hover_callback;
  std::function<void()> on_press_callback;
  std::function<void()> on_release_callback;
  std::function<void()> on_click_callback;
  std::function<void(const UIPointerButtonEvent &)>
      on_secondary_click_callback;
  std::function<void()> on_focus_callback;
  std::function<void()> on_blur_callback;
  std::function<void(const UIKeyInputEvent &)> on_key_input_callback;
  std::function<void(const UICharacterInputEvent &)>
      on_character_input_callback;
  std::function<void(const UIMouseWheelInputEvent &)> on_mouse_wheel_callback;
  std::function<void(const std::string &)> on_change_callback;
  std::function<void(const std::string &)> on_submit_callback;
  std::function<void(bool)> on_toggle_callback;
  std::function<void(float)> on_value_change_callback;
  std::function<void(size_t, const std::string &)> on_select_callback;
  std::function<void(size_t, const std::string &, bool)>
      on_chip_toggle_callback;

  NodeSpec &bind(UINodeId &target) {
    bound_id = &target;
    return *this;
  }

  NodeSpec &style() { return *this; }

  template <typename Rule, typename... Rest>
  NodeSpec &style(Rule &&rule, Rest &&...rest) {
    append_style(std::forward<Rule>(rule));
    if constexpr (sizeof...(rest) > 0) {
      style(std::forward<Rest>(rest)...);
    }

    return *this;
  }

private:
  void append_style(const StyleBuilder &builder) {
    builder.append_steps_to(style_steps);
  }

  void append_style(StyleBuilder &&builder) {
    std::move(builder).append_steps_to(style_steps);
  }

public:
  NodeSpec &children() { return *this; }

  template <typename Child, typename... Rest>
  NodeSpec &children(Child &&child, Rest &&...rest) {
    child_specs.emplace_back(std::forward<Child>(child));
    if constexpr (sizeof...(rest) > 0) {
      children(std::forward<Rest>(rest)...);
    }

    return *this;
  }

  NodeSpec &child(NodeSpec child) {
    child_specs.push_back(std::move(child));
    return *this;
  }

  NodeSpec &visible(bool value) {
    visible_value = value;
    return *this;
  }

  NodeSpec &enabled(bool value) {
    enabled_value = value;
    return *this;
  }

  NodeSpec &focusable(bool value) {
    focusable_value = value;
    return *this;
  }

  NodeSpec &read_only(bool value) {
    read_only_value = value;
    return *this;
  }

  NodeSpec &select_all_on_focus(bool value) {
    select_all_on_focus_value = value;
    return *this;
  }

  NodeSpec &checked(bool value) {
    checked_value = value;
    return *this;
  }

  NodeSpec &range(float min_value, float max_value) {
    slider_min_value = min_value;
    slider_max_value = max_value;
    return *this;
  }

  NodeSpec &step(float value) {
    slider_step_value = value;
    return *this;
  }

  NodeSpec &value(float next_value) {
    slider_value = next_value;
    return *this;
  }

  NodeSpec &options(std::vector<std::string> values) {
    option_values = std::move(values);
    return *this;
  }

  NodeSpec &accent_colors(std::vector<glm::vec4> colors) {
    item_accent_colors = std::move(colors);
    return *this;
  }

  NodeSpec &selected(size_t index) {
    selected_index_value = index;
    return *this;
  }

  NodeSpec &selected_chips(std::vector<bool> values) {
    chip_selected_values = std::move(values);
    return *this;
  }

  NodeSpec &on_hover(std::function<void()> callback) {
    on_hover_callback = std::move(callback);
    return *this;
  }

  NodeSpec &on_press(std::function<void()> callback) {
    on_press_callback = std::move(callback);
    return *this;
  }

  NodeSpec &on_release(std::function<void()> callback) {
    on_release_callback = std::move(callback);
    return *this;
  }

  NodeSpec &on_click(std::function<void()> callback) {
    on_click_callback = std::move(callback);
    return *this;
  }

  NodeSpec &on_secondary_click(
      std::function<void(const UIPointerButtonEvent &)> callback
  ) {
    on_secondary_click_callback = std::move(callback);
    return *this;
  }

  NodeSpec &on_focus(std::function<void()> callback) {
    on_focus_callback = std::move(callback);
    return *this;
  }

  NodeSpec &on_blur(std::function<void()> callback) {
    on_blur_callback = std::move(callback);
    return *this;
  }

  NodeSpec &
  on_key_input(std::function<void(const UIKeyInputEvent &)> callback) {
    on_key_input_callback = std::move(callback);
    return *this;
  }

  NodeSpec &on_character_input(
      std::function<void(const UICharacterInputEvent &)> callback
  ) {
    on_character_input_callback = std::move(callback);
    return *this;
  }

  NodeSpec &
  on_mouse_wheel(std::function<void(const UIMouseWheelInputEvent &)> callback) {
    on_mouse_wheel_callback = std::move(callback);
    return *this;
  }

  NodeSpec &on_change(std::function<void(const std::string &)> callback) {
    on_change_callback = std::move(callback);
    return *this;
  }

  NodeSpec &on_submit(std::function<void(const std::string &)> callback) {
    on_submit_callback = std::move(callback);
    return *this;
  }

  NodeSpec &on_toggle(std::function<void(bool)> callback) {
    on_toggle_callback = std::move(callback);
    return *this;
  }

  NodeSpec &on_value_change(std::function<void(float)> callback) {
    on_value_change_callback = std::move(callback);
    return *this;
  }

  NodeSpec &
  on_select(std::function<void(size_t, const std::string &)> callback) {
    on_select_callback = std::move(callback);
    return *this;
  }

  NodeSpec &on_chip_toggle(
      std::function<void(size_t, const std::string &, bool)> callback
  ) {
    on_chip_toggle_callback = std::move(callback);
    return *this;
  }
};

namespace detail {

inline bool spec_allows_children(NodeKind kind) {
  switch (kind) {
    case NodeKind::View:
    case NodeKind::Pressable:
    case NodeKind::ScrollView:
    case NodeKind::Popover:
      return true;
    case NodeKind::Text:
    case NodeKind::Image:
    case NodeKind::RenderImageView:
    case NodeKind::IconButton:
    case NodeKind::SegmentedControl:
    case NodeKind::ChipGroup:
    case NodeKind::TextInput:
    case NodeKind::Combobox:
    case NodeKind::Splitter:
    case NodeKind::Button:
    case NodeKind::Checkbox:
    case NodeKind::Slider:
    case NodeKind::Select:
    default:
      return false;
  }
}

inline void validate_spec(const NodeSpec &spec) {
  ASTRA_ENSURE(
      !spec_allows_children(spec.kind) && !spec.child_specs.empty(),
      "ui::dsl leaf nodes cannot declare children"
  );
}

#define ASTRA_NODE_BOOL_PROPERTIES(MAP)                   \
  MAP(visible_value, set_visible)                         \
  MAP(enabled_value, set_enabled)                         \
  MAP(focusable_value, set_focusable)                     \
  MAP(read_only_value, set_read_only)                     \
  MAP(select_all_on_focus_value, set_select_all_on_focus) \
  MAP(checked_value, set_checked)

#define ASTRA_NODE_CALLBACK_PROPERTIES(MAP)                \
  MAP(on_hover_callback, set_on_hover)                     \
  MAP(on_press_callback, set_on_press)                     \
  MAP(on_release_callback, set_on_release)                 \
  MAP(on_secondary_click_callback, set_on_secondary_click) \
  MAP(on_focus_callback, set_on_focus)                     \
  MAP(on_blur_callback, set_on_blur)                       \
  MAP(on_key_input_callback, set_on_key_input)             \
  MAP(on_character_input_callback, set_on_character_input) \
  MAP(on_mouse_wheel_callback, set_on_mouse_wheel)         \
  MAP(on_change_callback, set_on_change)                   \
  MAP(on_submit_callback, set_on_submit)                   \
  MAP(on_toggle_callback, set_on_toggle)                   \
  MAP(on_value_change_callback, set_on_value_change)       \
  MAP(on_select_callback, set_on_select)                   \
  MAP(on_chip_toggle_callback, set_on_chip_toggle)

inline void apply_properties(
    UIDocument &document,
    UINodeId node_id,
    const NodeSpec &spec
) {
  if (spec.bound_id != nullptr) {
    *spec.bound_id = node_id;
  }

#define APPLY_BOOL_PROPERTY(field, setter) \
  if (spec.field.has_value()) {            \
    document.setter(node_id, *spec.field); \
  }
  ASTRA_NODE_BOOL_PROPERTIES(APPLY_BOOL_PROPERTY)
#undef APPLY_BOOL_PROPERTY

  if (spec.slider_min_value.has_value() || spec.slider_max_value.has_value() ||
      spec.slider_step_value.has_value()) {
    document.set_slider_range(
        node_id,
        spec.slider_min_value.value_or(0.0f),
        spec.slider_max_value.value_or(1.0f),
        spec.slider_step_value.value_or(0.1f)
    );
  }

  if (spec.slider_value.has_value()) {
    document.set_slider_value(node_id, *spec.slider_value);
  }

  if (!spec.option_values.empty()) {
    document.set_combobox_options(node_id, spec.option_values);
    document.set_select_options(node_id, spec.option_values);
    document.set_segmented_options(node_id, spec.option_values);
    document.set_chip_options(
        node_id, spec.option_values, spec.chip_selected_values
    );
  }

  if (!spec.item_accent_colors.empty()) {
    document.set_segmented_item_accent_colors(
        node_id, spec.item_accent_colors
    );
  }

  if (spec.selected_index_value.has_value()) {
    document.set_selected_index(node_id, *spec.selected_index_value);
    document.set_segmented_selected_index(node_id, *spec.selected_index_value);
  }

  if (!spec.chip_selected_values.empty()) {
    document.set_chip_options(
        node_id, spec.option_values, spec.chip_selected_values
    );
  }

#define APPLY_CALLBACK_PROPERTY(field, setter) \
  if (spec.field) {                            \
    document.setter(node_id, spec.field);      \
  }
  ASTRA_NODE_CALLBACK_PROPERTIES(APPLY_CALLBACK_PROPERTY)
#undef APPLY_CALLBACK_PROPERTY

  if (spec.on_click_callback && spec.kind != NodeKind::Button) {
    document.set_on_click(node_id, spec.on_click_callback);
  }

  if (spec.line_chart_grid_line_count.has_value() ||
      spec.line_chart_grid_color.has_value()) {
    auto *target = document.node(node_id);
    if (target != nullptr) {
      if (spec.line_chart_grid_line_count.has_value()) {
        target->line_chart.grid_line_count = *spec.line_chart_grid_line_count;
      }
      if (spec.line_chart_grid_color.has_value()) {
        target->line_chart.grid_color = *spec.line_chart_grid_color;
      }
    }
  }

  if (!spec.style_steps.empty()) {
    document.mutate_style(node_id, [&spec](UIStyle &style) {
      for (const auto &step : spec.style_steps) {
        apply_style_step(style, step);
      }
    });
  }
}

#undef ASTRA_NODE_BOOL_PROPERTIES
#undef ASTRA_NODE_CALLBACK_PROPERTIES

} // namespace detail

} // namespace astralix::ui::dsl
