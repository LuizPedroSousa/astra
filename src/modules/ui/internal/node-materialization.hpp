#pragma once

#include "document/document.hpp"
#include "dsl/core.hpp"
#include "immediate/frame.hpp"

namespace astralix::ui::detail {

inline UINodeId create_common_node(
    UIDocument &document,
    dsl::NodeKind kind
) {
  switch (kind) {
    case dsl::NodeKind::View:
      return document.create_view();
    case dsl::NodeKind::Text:
      return document.create_text();
    case dsl::NodeKind::Image:
      return document.create_image();
    case dsl::NodeKind::RenderImageView:
      return document.create_render_image_view(RenderImageExportKey{});
    case dsl::NodeKind::Pressable:
      return document.create_pressable();
    case dsl::NodeKind::SegmentedControl:
      return document.create_segmented_control();
    case dsl::NodeKind::ChipGroup:
      return document.create_chip_group();
    case dsl::NodeKind::TextInput:
      return document.create_text_input();
    case dsl::NodeKind::Combobox:
      return document.create_combobox();
    case dsl::NodeKind::ScrollView:
      return document.create_scroll_view();
    case dsl::NodeKind::Popover:
      return document.create_popover();
    case dsl::NodeKind::Splitter:
      return document.create_splitter();
    case dsl::NodeKind::Checkbox:
      return document.create_checkbox();
    case dsl::NodeKind::Slider:
      return document.create_slider();
    case dsl::NodeKind::Select:
      return document.create_select();
    case dsl::NodeKind::LineChart:
      return document.create_line_chart();
    case dsl::NodeKind::IconButton:
      return document.create_icon_button();
    case dsl::NodeKind::Button:
      return document.create_button("", {});
  }

  return k_invalid_node_id;
}

inline bool node_supports_text(dsl::NodeKind kind) {
  switch (kind) {
    case dsl::NodeKind::Text:
    case dsl::NodeKind::TextInput:
    case dsl::NodeKind::Combobox:
    case dsl::NodeKind::Checkbox:
      return true;
    default:
      return false;
  }
}

inline bool node_supports_placeholder(dsl::NodeKind kind) {
  return kind == dsl::NodeKind::TextInput || kind == dsl::NodeKind::Combobox ||
         kind == dsl::NodeKind::Select;
}

inline bool node_supports_autocomplete(dsl::NodeKind kind) {
  return kind == dsl::NodeKind::TextInput || kind == dsl::NodeKind::Combobox;
}

inline bool vec4_equal(const glm::vec4 &lhs, const glm::vec4 &rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

inline bool optional_vec4_equal(
    const std::optional<glm::vec4> &lhs,
    const std::optional<glm::vec4> &rhs
) {
  if (lhs.has_value() != rhs.has_value()) {
    return false;
  }

  return !lhs.has_value() || vec4_equal(*lhs, *rhs);
}

inline bool length_equal(const UILength &lhs, const UILength &rhs) {
  return lhs.unit == rhs.unit && lhs.value == rhs.value;
}

inline bool edges_equal(const UIEdges &lhs, const UIEdges &rhs) {
  return lhs.left == rhs.left && lhs.top == rhs.top && lhs.right == rhs.right &&
         lhs.bottom == rhs.bottom;
}

inline bool state_style_equal(
    const UIStateStyle &lhs,
    const UIStateStyle &rhs
) {
  return optional_vec4_equal(lhs.background_color, rhs.background_color) &&
         optional_vec4_equal(lhs.border_color, rhs.border_color) &&
         lhs.border_width == rhs.border_width &&
         lhs.border_radius == rhs.border_radius &&
         lhs.opacity == rhs.opacity &&
         optional_vec4_equal(lhs.tint, rhs.tint) &&
         optional_vec4_equal(lhs.text_color, rhs.text_color);
}

inline bool style_equal(const UIStyle &lhs, const UIStyle &rhs) {
  return lhs.flex_direction == rhs.flex_direction &&
         lhs.justify_content == rhs.justify_content &&
         lhs.align_items == rhs.align_items &&
         lhs.align_self == rhs.align_self &&
         lhs.position_type == rhs.position_type &&
         lhs.overflow == rhs.overflow &&
         lhs.scroll_mode == rhs.scroll_mode &&
         lhs.scrollbar_visibility == rhs.scrollbar_visibility &&
         lhs.resize_mode == rhs.resize_mode &&
         lhs.resize_edges == rhs.resize_edges &&
         lhs.draggable == rhs.draggable &&
         lhs.drag_handle == rhs.drag_handle &&
         lhs.flex_grow == rhs.flex_grow &&
         lhs.flex_shrink == rhs.flex_shrink &&
         length_equal(lhs.flex_basis, rhs.flex_basis) &&
         length_equal(lhs.width, rhs.width) &&
         length_equal(lhs.height, rhs.height) &&
         length_equal(lhs.min_width, rhs.min_width) &&
         length_equal(lhs.min_height, rhs.min_height) &&
         length_equal(lhs.max_width, rhs.max_width) &&
         length_equal(lhs.max_height, rhs.max_height) &&
         length_equal(lhs.left, rhs.left) &&
         length_equal(lhs.top, rhs.top) &&
         length_equal(lhs.right, rhs.right) &&
         length_equal(lhs.bottom, rhs.bottom) &&
         edges_equal(lhs.margin, rhs.margin) &&
         edges_equal(lhs.padding, rhs.padding) &&
         lhs.gap == rhs.gap &&
         vec4_equal(lhs.background_color, rhs.background_color) &&
         vec4_equal(lhs.border_color, rhs.border_color) &&
         lhs.border_width == rhs.border_width &&
         lhs.border_radius == rhs.border_radius &&
         lhs.opacity == rhs.opacity &&
         vec4_equal(lhs.tint, rhs.tint) &&
         vec4_equal(lhs.text_color, rhs.text_color) &&
         lhs.font_id == rhs.font_id &&
         lhs.font_size == rhs.font_size &&
         lhs.line_height == rhs.line_height &&
         vec4_equal(lhs.placeholder_text_color, rhs.placeholder_text_color) &&
         vec4_equal(lhs.selection_color, rhs.selection_color) &&
         vec4_equal(lhs.caret_color, rhs.caret_color) &&
         lhs.scrollbar_thickness == rhs.scrollbar_thickness &&
         lhs.scrollbar_min_thumb_length == rhs.scrollbar_min_thumb_length &&
         vec4_equal(lhs.scrollbar_track_color, rhs.scrollbar_track_color) &&
         vec4_equal(lhs.scrollbar_thumb_color, rhs.scrollbar_thumb_color) &&
         vec4_equal(
             lhs.scrollbar_thumb_hovered_color,
             rhs.scrollbar_thumb_hovered_color
         ) &&
         vec4_equal(
             lhs.scrollbar_thumb_active_color,
             rhs.scrollbar_thumb_active_color
         ) &&
         lhs.resize_handle_thickness == rhs.resize_handle_thickness &&
         lhs.resize_corner_extent == rhs.resize_corner_extent &&
         vec4_equal(lhs.resize_handle_color, rhs.resize_handle_color) &&
         vec4_equal(
             lhs.resize_handle_hovered_color,
             rhs.resize_handle_hovered_color
         ) &&
         vec4_equal(
             lhs.resize_handle_active_color,
             rhs.resize_handle_active_color
         ) &&
         lhs.splitter_thickness == rhs.splitter_thickness &&
         vec4_equal(lhs.accent_color, rhs.accent_color) &&
         lhs.control_gap == rhs.control_gap &&
         lhs.control_indicator_size == rhs.control_indicator_size &&
         lhs.slider_track_thickness == rhs.slider_track_thickness &&
         lhs.slider_thumb_radius == rhs.slider_thumb_radius &&
         lhs.cursor == rhs.cursor &&
         state_style_equal(lhs.hovered_style, rhs.hovered_style) &&
         state_style_equal(lhs.pressed_style, rhs.pressed_style) &&
         state_style_equal(lhs.focused_style, rhs.focused_style) &&
         state_style_equal(lhs.disabled_style, rhs.disabled_style);
}

inline UIStyle materialize_style(
    const UIDocument::UINode &defaults,
    const im::Frame &frame,
    const im::NodeHeader &header
) {
  UIStyle style = defaults.style;
  for (uint32_t index = 0u; index < header.style_step_count; ++index) {
    dsl::apply_style_step(
        style,
        frame.style_step(header.first_style_step + index)
    );
  }
  return style;
}

inline void apply_common_properties(
    UIDocument &document,
    UINodeId node_id,
    const im::Frame &frame,
    const im::NodeHeader &header,
    const im::NodeState &state,
    const UIDocument::UINode &defaults
) {
  auto *target = document.node(node_id);
  if (target == nullptr) {
    return;
  }

  const UIStyle style = materialize_style(defaults, frame, header);
  if (!style_equal(target->style, style)) {
    document.set_style(node_id, style);
    target = document.node(node_id);
    if (target == nullptr) {
      return;
    }
  }

  const auto &text_payload = frame.text_payload(state);
  const auto &option_payload = frame.option_payload(state);
  const auto &callback_payload = frame.callback_payload(state);
  const auto &line_chart_payload = frame.line_chart_payload(state);
  const auto &popover_payload = frame.popover_payload(state);

  if (node_supports_text(header.kind)) {
    document.set_text(node_id, text_payload.text);
  }

  if (header.kind == dsl::NodeKind::Image) {
    document.set_texture(node_id, text_payload.texture_id);
  }

  if (header.kind == dsl::NodeKind::RenderImageView &&
      target->render_image_key != state.render_image_key) {
    target->render_image_key = state.render_image_key;
    document.mark_layout_dirty();
  }

  if (node_supports_placeholder(header.kind)) {
    document.set_placeholder(node_id, text_payload.placeholder);
  }

  if (node_supports_autocomplete(header.kind)) {
    document.set_autocomplete_text(node_id, text_payload.autocomplete_text);
  }

  document.set_visible(
      node_id,
      im::bool_value_or(header, im::NodeScalarField::Visible, defaults.visible)
  );
  document.set_enabled(
      node_id,
      im::bool_value_or(header, im::NodeScalarField::Enabled, defaults.enabled)
  );
  document.set_focusable(
      node_id,
      im::bool_value_or(
          header,
          im::NodeScalarField::Focusable,
          defaults.focusable
      )
  );
  document.set_read_only(
      node_id,
      im::bool_value_or(
          header,
          im::NodeScalarField::ReadOnly,
          defaults.read_only
      )
  );
  document.set_select_all_on_focus(
      node_id,
      im::bool_value_or(
          header,
          im::NodeScalarField::SelectAllOnFocus,
          defaults.select_all_on_focus
      )
  );
  document.set_checked(
      node_id,
      im::bool_value_or(
          header,
          im::NodeScalarField::Checked,
          defaults.checkbox.checked
      )
  );
  document.set_slider_range(
      node_id,
      im::value_or(
          header,
          im::NodeScalarField::SliderMin,
          state.slider_min_value,
          defaults.slider.min_value
      ),
      im::value_or(
          header,
          im::NodeScalarField::SliderMax,
          state.slider_max_value,
          defaults.slider.max_value
      ),
      im::value_or(
          header,
          im::NodeScalarField::SliderStep,
          state.slider_step_value,
          defaults.slider.step
      )
  );
  document.set_slider_value(
      node_id,
      im::value_or(
          header,
          im::NodeScalarField::SliderValue,
          state.slider_value,
          defaults.slider.value
      )
  );

  if (header.kind == dsl::NodeKind::Combobox) {
    document.set_combobox_options(node_id, option_payload.option_values);
    document.set_combobox_highlighted_index(
        node_id,
        im::value_or(
            header,
            im::NodeScalarField::HighlightedIndex,
            state.highlighted_index_value,
            defaults.combobox.highlighted_index
        )
    );
    document.set_combobox_open(
        node_id,
        im::bool_value_or(
            header,
            im::NodeScalarField::ComboboxOpen,
            defaults.combobox.open
        )
    );
  }

  if (header.kind == dsl::NodeKind::Select) {
    document.set_select_options(node_id, option_payload.option_values);
    document.set_selected_index(
        node_id,
        im::value_or(
            header,
            im::NodeScalarField::SelectedIndex,
            state.selected_index_value,
            defaults.select.selected_index
        )
    );
    document.set_select_open(
        node_id,
        im::bool_value_or(
            header,
            im::NodeScalarField::SelectOpen,
            defaults.select.open
        )
    );
  }

  if (header.kind == dsl::NodeKind::SegmentedControl) {
    document.set_segmented_options(node_id, option_payload.option_values);
    document.set_segmented_selected_index(
        node_id,
        im::value_or(
            header,
            im::NodeScalarField::SelectedIndex,
            state.selected_index_value,
            defaults.segmented_control.selected_index
        )
    );
    document.set_segmented_item_accent_colors(
        node_id,
        option_payload.item_accent_colors
    );
  }

  if (header.kind == dsl::NodeKind::ChipGroup) {
    document.set_chip_options(
        node_id,
        option_payload.option_values,
        option_payload.chip_selected_values
    );
  }

  document.set_on_hover(node_id, callback_payload.on_hover_callback);
  document.set_on_press(node_id, callback_payload.on_press_callback);
  document.set_on_release(node_id, callback_payload.on_release_callback);
  document.set_on_click(node_id, callback_payload.on_click_callback);
  document.set_on_secondary_click(
      node_id,
      callback_payload.on_secondary_click_callback
  );
  document.set_on_focus(node_id, callback_payload.on_focus_callback);
  document.set_on_blur(node_id, callback_payload.on_blur_callback);
  document.set_on_key_input(node_id, callback_payload.on_key_input_callback);
  document.set_on_character_input(
      node_id,
      callback_payload.on_character_input_callback
  );
  document.set_on_mouse_wheel(
      node_id,
      callback_payload.on_mouse_wheel_callback
  );
  document.set_on_change(node_id, callback_payload.on_change_callback);
  document.set_on_submit(node_id, callback_payload.on_submit_callback);
  document.set_on_toggle(node_id, callback_payload.on_toggle_callback);
  document.set_on_value_change(
      node_id,
      callback_payload.on_value_change_callback
  );
  document.set_on_select(node_id, callback_payload.on_select_callback);
  document.set_on_chip_toggle(
      node_id,
      callback_payload.on_chip_toggle_callback
  );

  if (header.kind == dsl::NodeKind::LineChart) {
    const bool auto_range =
        line_chart_payload.auto_range.value_or(defaults.line_chart.auto_range);
    document.set_line_chart_auto_range(node_id, auto_range);
    if (!auto_range) {
      document.set_line_chart_range(
          node_id,
          line_chart_payload.y_min.value_or(defaults.line_chart.y_min),
          line_chart_payload.y_max.value_or(defaults.line_chart.y_max)
      );
    }
    document.set_line_chart_series(
        node_id,
        line_chart_payload.has_series ? line_chart_payload.series
                                      : defaults.line_chart.series
    );
  }

  target = document.node(node_id);
  if (target != nullptr) {
    if (header.kind == dsl::NodeKind::Select) {
      const size_t highlighted_index =
          im::value_or(
              header,
              im::NodeScalarField::HighlightedIndex,
              state.highlighted_index_value,
              defaults.select.highlighted_index
          );
      if (target->select.highlighted_index != highlighted_index) {
        target->select.highlighted_index = highlighted_index;
        document.mark_paint_dirty();
      }
    }

    if (header.kind == dsl::NodeKind::Combobox) {
      const bool open_on_arrow_keys =
          im::bool_value_or(
              header,
              im::NodeScalarField::ComboboxOpenOnArrowKeys,
              defaults.combobox.open_on_arrow_keys
          );
      if (target->combobox.open_on_arrow_keys != open_on_arrow_keys) {
        target->combobox.open_on_arrow_keys = open_on_arrow_keys;
      }
    }

    if (header.kind == dsl::NodeKind::Popover) {
      const bool close_on_escape = popover_payload.state.has_value()
                                       ? popover_payload.state->close_on_escape
                                       : defaults.popover.close_on_escape;
      const bool close_on_outside_click =
          popover_payload.state.has_value()
              ? popover_payload.state->close_on_outside_click
              : defaults.popover.close_on_outside_click;
      target->popover.close_on_escape = close_on_escape;
      target->popover.close_on_outside_click = close_on_outside_click;
    }

    if (header.kind == dsl::NodeKind::LineChart) {
      const size_t grid_line_count =
          line_chart_payload.grid_line_count.value_or(
              defaults.line_chart.grid_line_count
          );
      const glm::vec4 grid_color =
          line_chart_payload.grid_color.value_or(defaults.line_chart.grid_color);
      if (target->line_chart.grid_line_count != grid_line_count ||
          !vec4_equal(target->line_chart.grid_color, grid_color)) {
        target->line_chart.grid_line_count = grid_line_count;
        target->line_chart.grid_color = grid_color;
        document.mark_paint_dirty();
      }
    }
  }
}

} // namespace astralix::ui::detail
