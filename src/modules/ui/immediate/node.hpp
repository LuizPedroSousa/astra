#pragma once

#include "dsl/core.hpp"
#include "fnv1a.hpp"
#include "immediate/id.hpp"

#include <limits>

namespace astralix::ui::im {

using NodeId = uint32_t;
constexpr NodeId k_invalid_immediate_node_id =
    std::numeric_limits<NodeId>::max();
using PayloadIndex = uint32_t;
constexpr PayloadIndex k_invalid_immediate_payload_index =
    std::numeric_limits<PayloadIndex>::max();
using NodeScalarMask = uint16_t;

struct PopoverState {
  bool open = false;
  WidgetId anchor_widget_id = k_invalid_widget_id;
  std::optional<glm::vec2> anchor_point;
  UIPopupPlacement placement = UIPopupPlacement::BottomStart;
  size_t depth = 0u;
  bool close_on_outside_click = true;
  bool close_on_escape = true;
};

struct NodeTextPayload {
  std::string debug_name;
  std::string text;
  std::string placeholder;
  std::string autocomplete_text;
  ResourceDescriptorID texture_id;
};

struct NodeOptionPayload {
  std::vector<std::string> option_values;
  std::vector<glm::vec4> item_accent_colors;
  std::vector<bool> chip_selected_values;
};

struct NodeLineChartPayload {
  std::optional<size_t> grid_line_count;
  std::optional<glm::vec4> grid_color;
  bool has_series = false;
  std::vector<UILineChartSeries> series;
  std::optional<bool> auto_range;
  std::optional<float> y_min;
  std::optional<float> y_max;
};

struct NodePopoverPayload {
  std::optional<PopoverState> state;
};

struct NodeCallbackPayload {
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
};

enum class NodeScalarField : NodeScalarMask {
  Visible = 1u << 0,
  Enabled = 1u << 1,
  Focusable = 1u << 2,
  ReadOnly = 1u << 3,
  SelectAllOnFocus = 1u << 4,
  Checked = 1u << 5,
  SliderValue = 1u << 6,
  SliderMin = 1u << 7,
  SliderMax = 1u << 8,
  SliderStep = 1u << 9,
  SelectedIndex = 1u << 10,
  HighlightedIndex = 1u << 11,
  ComboboxOpen = 1u << 12,
  SelectOpen = 1u << 13,
  ComboboxOpenOnArrowKeys = 1u << 14,
  Collapsed = 1u << 15,
};

constexpr NodeScalarMask node_scalar_bit(NodeScalarField field) {
  return static_cast<NodeScalarMask>(field);
}

struct NodeLinkChain {
  NodeId first_child_id = k_invalid_immediate_node_id;
  NodeId last_child_id = k_invalid_immediate_node_id;
  NodeId next_sibling_id = k_invalid_immediate_node_id;
};

struct NodeHeader {
  dsl::NodeKind kind = dsl::NodeKind::View;
  bool frozen = false;
  WidgetId widget_id = k_invalid_widget_id;
  uint32_t child_count = 0u;
  uint32_t first_child_offset = 0u;
  uint32_t first_style_step = 0u;
  uint32_t style_step_count = 0u;
  NodeScalarMask scalar_mask = 0u;
  NodeScalarMask bool_mask = 0u;
};

struct NodeState {
  PayloadIndex text_payload_index = k_invalid_immediate_payload_index;
  PayloadIndex option_payload_index = k_invalid_immediate_payload_index;
  PayloadIndex line_chart_payload_index = k_invalid_immediate_payload_index;
  PayloadIndex popover_payload_index = k_invalid_immediate_payload_index;
  PayloadIndex callback_payload_index = k_invalid_immediate_payload_index;
  std::optional<RenderImageExportKey> render_image_key;
  float slider_value = 0.0f;
  float slider_min_value = 0.0f;
  float slider_max_value = 0.0f;
  float slider_step_value = 0.0f;
  size_t selected_index_value = 0u;
  size_t highlighted_index_value = 0u;
};

inline bool has_scalar_value(
    const NodeHeader &node,
    NodeScalarField field
) {
  return (node.scalar_mask & node_scalar_bit(field)) != 0u;
}

inline bool bool_value_or(
    const NodeHeader &node,
    NodeScalarField field,
    bool default_value
) {
  if (!has_scalar_value(node, field)) {
    return default_value;
  }

  return (node.bool_mask & node_scalar_bit(field)) != 0u;
}

template <typename T>
inline T value_or(
    const NodeHeader &node,
    NodeScalarField field,
    const T &value,
    T default_value
) {
  return has_scalar_value(node, field) ? value : default_value;
}

inline uint64_t hash_style_step_span(
    const dsl::StyleStepStorage &steps,
    uint32_t first_step,
    uint32_t step_count
) {
  uint64_t hash = k_fnv1a64_offset_basis;
  for (uint32_t index = 0u; index < step_count; ++index) {
    const auto &step = steps[first_step + index];
    if (const auto *operation =
            std::get_if<dsl::StyleOp>(&step)) {
      hash = fnv1a64_append_value(hash, operation->target);
      hash = fnv1a64_append_value(hash, operation->code);
      hash = fnv1a64_append_bytes(
          hash, &operation->payload, sizeof(operation->payload)
      );
    } else if (const auto *font_step =
                   std::get_if<dsl::FontIdStyleStep>(&step)) {
      constexpr uint8_t tag = 1u;
      hash = fnv1a64_append_value(hash, tag);
      hash = fnv1a64_append_string(font_step->value, hash);
    }
  }
  return hash;
}

} // namespace astralix::ui::im
