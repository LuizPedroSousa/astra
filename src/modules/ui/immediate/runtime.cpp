#include "immediate/runtime.hpp"

#include "internal/node-materialization.hpp"
#include "log.hpp"
#include "trace.hpp"

#include <algorithm>

namespace astralix::ui::im {

Frame::Frame(
    Runtime *runtime,
    BumpAllocator *allocator,
    WidgetId root_scope_id,
    size_t previous_node_count
)
    : Children(this, k_invalid_immediate_node_id, root_scope_id),
      m_runtime(runtime), m_allocator(allocator),
      m_header_nodes(allocator), m_state_nodes(allocator),
      m_link_chains(allocator), m_child_ids(allocator),
      m_text_payloads(allocator),
      m_option_payloads(allocator),
      m_line_chart_payloads(allocator), m_graph_payloads(allocator),
      m_popover_payloads(allocator),
      m_callback_payloads(allocator) {
  const size_t node_reserve =
      previous_node_count > 0u
          ? previous_node_count + previous_node_count / 4u
          : 256u;
  m_header_nodes.reserve(node_reserve);
  m_state_nodes.reserve(node_reserve);
  m_link_chains.reserve(node_reserve);
  m_style_steps.reserve(1024u);
  m_text_payloads.reserve(std::max<size_t>(128u, node_reserve / 2u));
  m_option_payloads.reserve(32u);
  m_line_chart_payloads.reserve(8u);
  m_graph_payloads.reserve(8u);
  m_popover_payloads.reserve(8u);
  m_callback_payloads.reserve(std::max<size_t>(32u, node_reserve / 4u));
  m_root_node_id = create_node(dsl::NodeKind::View, root_scope_id);
  m_parent_id = m_root_node_id;
  m_focus_requests.reserve(8u);
  m_pointer_capture_requests.reserve(4u);
  m_pointer_capture_release_requests.reserve(4u);
  m_text_selection_requests.reserve(4u);
  m_caret_requests.reserve(4u);
  m_scroll_requests.reserve(4u);
  m_view_transform_requests.reserve(4u);
  m_auto_child_counters.reserve(std::max<size_t>(32u, node_reserve / 2u));
}

VirtualListState Frame::virtual_list_state(WidgetId widget_id) const {
  return m_runtime != nullptr ? m_runtime->virtual_list_state(widget_id)
                              : VirtualListState{};
}

std::vector<Frame::LazySectionEntry> &Frame::lazy_section_registry(
    WidgetId scroll_parent_id
) {
  return m_lazy_section_registries[scroll_parent_id];
}

void Frame::flatten_children() {
  const size_t node_count = m_header_nodes.size();
  size_t total_children = 0u;
  for (size_t index = 0u; index < node_count; ++index) {
    total_children += m_header_nodes[index].child_count;
  }
  m_child_ids.reserve(total_children);

  for (size_t index = 0u; index < node_count; ++index) {
    NodeHeader &header = m_header_nodes[index];
    if (header.child_count == 0u) {
      continue;
    }
    header.first_child_offset = static_cast<uint32_t>(m_child_ids.size());
    const NodeLinkChain &links = m_link_chains[index];
    for (NodeId child = links.first_child_id;
         child != k_invalid_immediate_node_id;
         child = m_link_chains[child].next_sibling_id) {
      m_child_ids.push_back(child);
    }
  }
}

float Frame::measured_node_height(WidgetId widget_id) const {
  return m_runtime != nullptr ? m_runtime->measured_node_height(widget_id)
                              : 0.0f;
}

Runtime::Runtime(Ref<UIDocument> document, UINodeId host_node_id)
    : m_document(std::move(document)),
      m_host_node_id(host_node_id),
      m_root_scope_id(WidgetId{
          static_cast<uint64_t>(host_node_id == k_invalid_node_id ? 1u : host_node_id),
      }) {}

void Runtime::pre_reserve_frame_allocator() {
  constexpr size_t k_min_arena_bytes = 32u * 1024u;
  const size_t estimated_node_bytes =
      (m_last_frame_node_count + m_last_frame_node_count / 4u + 64u) *
      (sizeof(NodeHeader) + sizeof(NodeState) + sizeof(NodeLinkChain));
  const size_t target =
      std::max(k_min_arena_bytes, estimated_node_bytes * 2u);
  m_frame_allocator.reserve(target);
}

VirtualListState Runtime::virtual_list_state(WidgetId widget_id) const {
  const UINodeId node_id = node_id_for(widget_id);
  const auto *node = m_document != nullptr ? m_document->node(node_id) : nullptr;
  if (node == nullptr) {
    return {};
  }

  return VirtualListState{
      .scroll_offset = node->layout.scroll.offset,
      .viewport_height = node->layout.scroll.viewport_size.y,
      .viewport_width = node->layout.scroll.viewport_size.x,
  };
}

std::optional<UIViewTransform2D>
Runtime::view_transform(WidgetId widget_id) const {
  const UINodeId node_id = node_id_for(widget_id);
  return m_document != nullptr ? m_document->view_transform(node_id)
                               : std::nullopt;
}

std::optional<UIGraphSelection>
Runtime::graph_selection(WidgetId widget_id) const {
  const UINodeId node_id = node_id_for(widget_id);
  const auto *node = m_document != nullptr ? m_document->node(node_id) : nullptr;
  if (node == nullptr || node->type != NodeType::GraphView) {
    return std::nullopt;
  }

  return node->graph_view.model.selection;
}

float Runtime::measured_node_height(WidgetId widget_id) const {
  const UINodeId node_id = node_id_for(widget_id);
  const auto *node = m_document != nullptr ? m_document->node(node_id) : nullptr;
  if (node == nullptr || node->layout.bounds.height <= 0.0f) {
    return 0.0f;
  }
  return node->layout.bounds.height;
}

std::optional<UIRect> Runtime::layout_bounds(WidgetId widget_id) const {
  const UINodeId node_id = node_id_for(widget_id);
  const auto *node = m_document != nullptr ? m_document->node(node_id) : nullptr;
  if (node == nullptr || !node->visible || node->layout.bounds.width <= 0.0f ||
      node->layout.bounds.height <= 0.0f) {
    return std::nullopt;
  }

  return node->layout.bounds;
}

bool Runtime::combobox_open(WidgetId widget_id) const {
  const UINodeId node_id = node_id_for(widget_id);
  return m_document != nullptr ? m_document->combobox_open(node_id) : false;
}

size_t Runtime::combobox_highlighted_index(WidgetId widget_id) const {
  const UINodeId node_id = node_id_for(widget_id);
  return m_document != nullptr ? m_document->combobox_highlighted_index(node_id)
                               : 0u;
}

UINodeId Runtime::node_id_for(WidgetId widget_id) const {
  const auto it = m_widget_nodes.find(widget_id);
  return it != m_widget_nodes.end() ? it->second.node_id : k_invalid_node_id;
}

void Runtime::reconcile_host_children(const Frame &frame) {
  ASTRA_PROFILE_N("im::Runtime::reconcile_host_children");
  const NodeHeader *root = frame.header(frame.root_node_id());
  ASTRA_ENSURE(
      root == nullptr,
      "ui::im::Runtime cannot reconcile an invalid frame root"
  );
  reconcile_children(
      m_host_node_id,
      frame,
      root->first_child_offset,
      root->child_count
  );
}

UINodeId Runtime::create_node(const NodeHeader &header) {
  const UINodeId node_id =
      ui::detail::create_common_node(*m_document, header.kind);
  ASTRA_ENSURE(
      node_id == k_invalid_node_id,
      "ui::im::Runtime failed to create node"
  );
  NodeRuntime runtime_state{
      .node_id = node_id,
      .kind = header.kind,
  };
  if (const auto *created = m_document->node(node_id); created != nullptr) {
    runtime_state.defaults = *created;
  }
  m_widget_nodes[header.widget_id] = runtime_state;
  if (node_id >= m_node_widgets.size()) {
    m_node_widgets.resize(node_id + 1u, k_invalid_widget_id);
  }
  m_node_widgets[node_id] = header.widget_id;
  if (node_id >= m_live_nodes.size()) {
    m_live_nodes.resize(node_id + 1u, false);
  }
  m_live_nodes[node_id] = true;
  install_callback_forwarders(node_id, header.widget_id);
  return node_id;
}

void Runtime::install_callback_forwarders(
    UINodeId node_id,
    WidgetId widget_id
) {
  auto &slot = m_callback_slots[widget_id];
  if (!slot) {
    slot = std::make_unique<CallbackSlot>();
  }
  CallbackSlot *slot_ptr = slot.get();

  m_document->set_on_hover(node_id, [slot_ptr]() {
    if (slot_ptr->payload.on_hover_callback)
      slot_ptr->payload.on_hover_callback();
  });
  m_document->set_on_press(node_id, [slot_ptr]() {
    if (slot_ptr->payload.on_press_callback)
      slot_ptr->payload.on_press_callback();
  });
  m_document->set_on_release(node_id, [slot_ptr]() {
    if (slot_ptr->payload.on_release_callback)
      slot_ptr->payload.on_release_callback();
  });
  m_document->set_on_click(node_id, [slot_ptr]() {
    if (slot_ptr->payload.on_click_callback)
      slot_ptr->payload.on_click_callback();
  });
  m_document->set_on_secondary_click(
      node_id,
      [slot_ptr](const UIPointerButtonEvent &event) {
        if (slot_ptr->payload.on_secondary_click_callback)
          slot_ptr->payload.on_secondary_click_callback(event);
      }
  );
  m_document->set_on_focus(node_id, [slot_ptr]() {
    if (slot_ptr->payload.on_focus_callback)
      slot_ptr->payload.on_focus_callback();
  });
  m_document->set_on_blur(node_id, [slot_ptr]() {
    if (slot_ptr->payload.on_blur_callback)
      slot_ptr->payload.on_blur_callback();
  });
  m_document->set_on_key_input(
      node_id,
      [slot_ptr](const UIKeyInputEvent &event) {
        if (slot_ptr->payload.on_key_input_callback)
          slot_ptr->payload.on_key_input_callback(event);
      }
  );
  m_document->set_on_character_input(
      node_id,
      [slot_ptr](const UICharacterInputEvent &event) {
        if (slot_ptr->payload.on_character_input_callback)
          slot_ptr->payload.on_character_input_callback(event);
      }
  );
  m_document->set_on_mouse_wheel(
      node_id,
      [slot_ptr](const UIMouseWheelInputEvent &event) {
        if (slot_ptr->payload.on_mouse_wheel_callback)
          slot_ptr->payload.on_mouse_wheel_callback(event);
      }
  );
  m_document->set_on_view_transform_change(
      node_id,
      [slot_ptr](const UIViewTransformChangeEvent &event) {
        if (slot_ptr->payload.on_view_transform_change_callback) {
          slot_ptr->payload.on_view_transform_change_callback(event);
        }
      }
  );
  m_document->set_on_change(
      node_id,
      [slot_ptr](const std::string &value) {
        if (slot_ptr->payload.on_change_callback)
          slot_ptr->payload.on_change_callback(value);
      }
  );
  m_document->set_on_submit(
      node_id,
      [slot_ptr](const std::string &value) {
        if (slot_ptr->payload.on_submit_callback)
          slot_ptr->payload.on_submit_callback(value);
      }
  );
  m_document->set_on_toggle(node_id, [slot_ptr](bool value) {
    if (slot_ptr->payload.on_toggle_callback)
      slot_ptr->payload.on_toggle_callback(value);
  });
  m_document->set_on_value_change(node_id, [slot_ptr](float value) {
    if (slot_ptr->payload.on_value_change_callback)
      slot_ptr->payload.on_value_change_callback(value);
  });
  m_document->set_on_select(
      node_id,
      [slot_ptr](size_t index, const std::string &value) {
        if (slot_ptr->payload.on_select_callback)
          slot_ptr->payload.on_select_callback(index, value);
      }
  );
  m_document->set_on_chip_toggle(
      node_id,
      [slot_ptr](size_t index, const std::string &value, bool selected) {
        if (slot_ptr->payload.on_chip_toggle_callback)
          slot_ptr->payload.on_chip_toggle_callback(index, value, selected);
      }
  );

  m_document->set_on_graph_selection_change(
      node_id,
      [slot_ptr](const UIGraphSelection &selection) {
        if (slot_ptr->payload.on_graph_selection_change_callback) {
          slot_ptr->payload.on_graph_selection_change_callback(selection);
        }
      }
  );
  m_document->set_on_graph_node_move(
      node_id,
      [slot_ptr](UIGraphId node_id, glm::vec2 position) {
        if (slot_ptr->payload.on_graph_node_move_callback) {
          slot_ptr->payload.on_graph_node_move_callback(node_id, position);
        }
      }
  );
  m_document->set_on_graph_connection_drag_end(
      node_id,
      [slot_ptr](UIGraphId from_port_id, std::optional<UIGraphId> to_port_id) {
        if (slot_ptr->payload.on_graph_connection_drag_end_callback) {
          slot_ptr->payload.on_graph_connection_drag_end_callback(
              from_port_id,
              to_port_id
          );
        }
      }
  );
}

void Runtime::apply_node(
    UINodeId node_id,
    const Frame &frame,
    const NodeHeader &header,
    const NodeState &state
) {
  ASTRA_PROFILE_N("im::Runtime::apply_node");
  auto widget_it = m_widget_nodes.find(header.widget_id);
  ASTRA_ENSURE(
      widget_it == m_widget_nodes.end(),
      "ui::im::Runtime lost widget state for a live node"
  );

  auto *target = m_document->node(node_id);
  if (target == nullptr) {
    return;
  }

  const uint64_t style_hash = hash_style_step_span(
      frame.style_steps(), header.first_style_step, header.style_step_count
  );
  if (style_hash != widget_it->second.last_style_hash) {
    const UIStyle style = ui::detail::materialize_style(
        widget_it->second.defaults, frame, header
    );
    if (!ui::detail::style_equal(target->style, style)) {
      m_document->set_style(node_id, style);
      target = m_document->node(node_id);
      if (target == nullptr) {
        return;
      }
    }
    widget_it->second.last_style_hash = style_hash;
  }

  const auto &text_payload = frame.text_payload(state);
  const auto &option_payload = frame.option_payload(state);
  const auto &line_chart_payload = frame.line_chart_payload(state);
  const auto &graph_payload = frame.graph_payload(state);

  if (ui::detail::node_supports_text(header.kind)) {
    m_document->set_text(node_id, text_payload.text);
  }

  if (header.kind == dsl::NodeKind::Image) {
    m_document->set_texture(node_id, text_payload.texture_id);
  }

  if (header.kind == dsl::NodeKind::RenderImageView &&
      target->render_image_key != state.render_image_key) {
    target->render_image_key = state.render_image_key;
    m_document->mark_layout_dirty();
  }

  if (ui::detail::node_supports_placeholder(header.kind)) {
    m_document->set_placeholder(node_id, text_payload.placeholder);
  }

  if (ui::detail::node_supports_autocomplete(header.kind)) {
    m_document->set_autocomplete_text(node_id, text_payload.autocomplete_text);
  }

  const auto &defaults = widget_it->second.defaults;
  m_document->set_visible(
      node_id,
      im::bool_value_or(header, im::NodeScalarField::Visible, defaults.visible)
  );
  m_document->set_enabled(
      node_id,
      im::bool_value_or(header, im::NodeScalarField::Enabled, defaults.enabled)
  );
  m_document->set_focusable(
      node_id,
      im::bool_value_or(
          header, im::NodeScalarField::Focusable, defaults.focusable
      )
  );
  m_document->set_read_only(
      node_id,
      im::bool_value_or(
          header, im::NodeScalarField::ReadOnly, defaults.read_only
      )
  );
  m_document->set_select_all_on_focus(
      node_id,
      im::bool_value_or(
          header,
          im::NodeScalarField::SelectAllOnFocus,
          defaults.select_all_on_focus
      )
  );
  m_document->set_checked(
      node_id,
      im::bool_value_or(
          header, im::NodeScalarField::Checked, defaults.checkbox.checked
      )
  );
  m_document->set_slider_range(
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
  m_document->set_slider_value(
      node_id,
      im::value_or(
          header,
          im::NodeScalarField::SliderValue,
          state.slider_value,
          defaults.slider.value
      )
  );

  if (header.kind == dsl::NodeKind::Combobox) {
    m_document->set_combobox_options(node_id, option_payload.option_values);
    m_document->set_combobox_highlighted_index(
        node_id,
        im::value_or(
            header,
            im::NodeScalarField::HighlightedIndex,
            state.highlighted_index_value,
            defaults.combobox.highlighted_index
        )
    );
    m_document->set_combobox_open(
        node_id,
        im::bool_value_or(
            header,
            im::NodeScalarField::ComboboxOpen,
            defaults.combobox.open
        )
    );
  }

  if (header.kind == dsl::NodeKind::Select) {
    m_document->set_select_options(node_id, option_payload.option_values);
    m_document->set_selected_index(
        node_id,
        im::value_or(
            header,
            im::NodeScalarField::SelectedIndex,
            state.selected_index_value,
            defaults.select.selected_index
        )
    );
    m_document->set_select_open(
        node_id,
        im::bool_value_or(
            header, im::NodeScalarField::SelectOpen, defaults.select.open
        )
    );
  }

  m_document->set_view_transform_enabled(
      node_id,
      im::bool_value_or(
          header,
          im::NodeScalarField::ViewTransformEnabled,
          defaults.view_transform_interaction.enabled
      )
  );
  m_document->set_view_transform_middle_mouse_pan(
      node_id,
      im::bool_value_or(
          header,
          im::NodeScalarField::ViewTransformMiddleMousePan,
          defaults.view_transform_interaction.middle_mouse_pan
      )
  );
  m_document->set_view_transform_wheel_zoom(
      node_id,
      im::bool_value_or(
          header,
          im::NodeScalarField::ViewTransformWheelZoom,
          defaults.view_transform_interaction.wheel_zoom
      )
  );

  if (header.kind == dsl::NodeKind::SegmentedControl) {
    m_document->set_segmented_options(node_id, option_payload.option_values);
    m_document->set_segmented_selected_index(
        node_id,
        im::value_or(
            header,
            im::NodeScalarField::SelectedIndex,
            state.selected_index_value,
            defaults.segmented_control.selected_index
        )
    );
    m_document->set_segmented_item_accent_colors(
        node_id, option_payload.item_accent_colors
    );
  }

  if (header.kind == dsl::NodeKind::ChipGroup) {
    m_document->set_chip_options(
        node_id,
        option_payload.option_values,
        option_payload.chip_selected_values
    );
  }

  if (state.callback_payload_index != k_invalid_immediate_payload_index) {
    auto slot_it = m_callback_slots.find(header.widget_id);
    if (slot_it != m_callback_slots.end()) {
      slot_it->second->payload = frame.callback_payload(state);
    }
  } else {
    auto slot_it = m_callback_slots.find(header.widget_id);
    if (slot_it != m_callback_slots.end()) {
      slot_it->second->payload = {};
    }
  }

  auto slot_it = m_callback_slots.find(header.widget_id);
  CallbackSlot *slot_ptr =
      slot_it != m_callback_slots.end() ? slot_it->second.get() : nullptr;
  if (header.kind != dsl::NodeKind::GraphView && slot_ptr != nullptr &&
      slot_ptr->payload.on_pointer_event_callback) {
    m_document->set_on_pointer_event(
        node_id,
        [slot_ptr](const UIPointerEvent &event) {
          if (slot_ptr->payload.on_pointer_event_callback) {
            slot_ptr->payload.on_pointer_event_callback(event);
          }
        }
    );
  } else if (header.kind != dsl::NodeKind::GraphView) {
    m_document->set_on_pointer_event(node_id, {});
  }

  if (header.kind != dsl::NodeKind::GraphView && slot_ptr != nullptr &&
      slot_ptr->payload.on_custom_hit_test_callback) {
    m_document->set_on_custom_hit_test(
        node_id,
        [slot_ptr](glm::vec2 local_position)
            -> std::optional<UICustomHitData> {
          if (slot_ptr->payload.on_custom_hit_test_callback) {
            return slot_ptr->payload.on_custom_hit_test_callback(
                local_position
            );
          }
          return std::nullopt;
        }
    );
  } else if (header.kind != dsl::NodeKind::GraphView) {
    m_document->set_on_custom_hit_test(node_id, {});
  }

  if (header.kind == dsl::NodeKind::LineChart) {
    const bool auto_range =
        line_chart_payload.auto_range.value_or(defaults.line_chart.auto_range);
    m_document->set_line_chart_auto_range(node_id, auto_range);
    if (!auto_range) {
      m_document->set_line_chart_range(
          node_id,
          line_chart_payload.y_min.value_or(defaults.line_chart.y_min),
          line_chart_payload.y_max.value_or(defaults.line_chart.y_max)
      );
    }
    m_document->set_line_chart_series(
        node_id,
        line_chart_payload.has_series ? line_chart_payload.series
                                      : defaults.line_chart.series
    );
  } else if (header.kind == dsl::NodeKind::GraphView) {
    m_document->set_graph_view_model(
        node_id,
        graph_payload.spec.model
    );
  }

  target = m_document->node(node_id);
  if (target != nullptr) {
    switch (header.kind) {
      case dsl::NodeKind::Select: {
        const size_t highlighted_index = im::value_or(
            header,
            im::NodeScalarField::HighlightedIndex,
            state.highlighted_index_value,
            defaults.select.highlighted_index
        );
        if (target->select.highlighted_index != highlighted_index) {
          target->select.highlighted_index = highlighted_index;
          m_document->mark_paint_dirty();
        }
        break;
      }
      case dsl::NodeKind::Combobox: {
        const bool open_on_arrow_keys = im::bool_value_or(
            header,
            im::NodeScalarField::ComboboxOpenOnArrowKeys,
            defaults.combobox.open_on_arrow_keys
        );
        if (target->combobox.open_on_arrow_keys != open_on_arrow_keys) {
          target->combobox.open_on_arrow_keys = open_on_arrow_keys;
        }
        break;
      }
      case dsl::NodeKind::Popover: {
        const auto &popover_payload = frame.popover_payload(state);
        const bool close_on_escape =
            popover_payload.state.has_value()
                ? popover_payload.state->close_on_escape
                : defaults.popover.close_on_escape;
        const bool close_on_outside_click =
            popover_payload.state.has_value()
                ? popover_payload.state->close_on_outside_click
                : defaults.popover.close_on_outside_click;
        target->popover.close_on_escape = close_on_escape;
        target->popover.close_on_outside_click = close_on_outside_click;
        break;
      }
      case dsl::NodeKind::LineChart: {
        const size_t grid_line_count =
            line_chart_payload.grid_line_count.value_or(
                defaults.line_chart.grid_line_count
            );
        const glm::vec4 grid_color =
            line_chart_payload.grid_color.value_or(
                defaults.line_chart.grid_color
            );
        if (target->line_chart.grid_line_count != grid_line_count ||
            !ui::detail::vec4_equal(target->line_chart.grid_color, grid_color)) {
          target->line_chart.grid_line_count = grid_line_count;
          target->line_chart.grid_color = grid_color;
          m_document->mark_paint_dirty();
        }
        break;
      }
      default:
        break;
    }
  }

  const auto &popover_payload = frame.popover_payload(state);
  if (header.kind == dsl::NodeKind::Popover && popover_payload.state.has_value()) {
    apply_popover_state(node_id, *popover_payload.state);
  }
}

void Runtime::apply_popover_state(
    UINodeId node_id,
    const PopoverState &state
) {
  if (!state.open) {
    m_document->close_popover(node_id);
    return;
  }

  if (state.anchor_widget_id) {
    const UINodeId anchor_node_id = node_id_for(state.anchor_widget_id);
    if (anchor_node_id != k_invalid_node_id) {
      m_document->open_popover_anchored_to(
          node_id,
          anchor_node_id,
          state.placement,
          state.depth
      );
    }
    return;
  }

  if (state.anchor_point.has_value()) {
    m_document->open_popover_at(
        node_id,
        *state.anchor_point,
        state.placement,
        state.depth
    );
  }
}

void Runtime::destroy_runtime_subtree(UINodeId node_id) {
  erase_mapping_for_subtree(node_id);
  m_document->destroy_subtree(node_id);
}

void Runtime::erase_mapping_for_subtree(UINodeId node_id) {
  auto erase_single = [this](UINodeId target_id) {
    if (target_id < m_node_widgets.size() &&
        m_node_widgets[target_id] != k_invalid_widget_id) {
      const WidgetId widget_id = m_node_widgets[target_id];
      m_callback_slots.erase(widget_id);
      m_widget_nodes.erase(widget_id);
      m_node_widgets[target_id] = k_invalid_widget_id;
    }
    if (target_id < m_live_nodes.size()) {
      m_live_nodes[target_id] = false;
    }
  };

  const auto *node = m_document->node(node_id);
  if (node == nullptr) {
    erase_single(node_id);
    return;
  }

  std::vector<UINodeId> stack{node_id};
  while (!stack.empty()) {
    const UINodeId current_id = stack.back();
    stack.pop_back();

    const auto *current = m_document->node(current_id);
    if (current != nullptr) {
      for (const auto child_id : current->children) {
        stack.push_back(child_id);
      }
    }

    erase_single(current_id);
  }
}

UINodeId Runtime::reconcile_node(const Frame &frame, NodeId immediate_node_id) {
  const NodeHeader *node_header = frame.header(immediate_node_id);
  ASTRA_ENSURE(
      node_header == nullptr,
      "ui::im::Runtime cannot reconcile an invalid immediate node"
  );

  UINodeId node_id = k_invalid_node_id;
  const auto existing_it = m_widget_nodes.find(node_header->widget_id);
  if (existing_it != m_widget_nodes.end() &&
      existing_it->second.kind == node_header->kind &&
      m_document->node(existing_it->second.node_id) != nullptr) {
    node_id = existing_it->second.node_id;
  } else {
    if (existing_it != m_widget_nodes.end()) {
      destroy_runtime_subtree(existing_it->second.node_id);
    }
    node_id = create_node(*node_header);
  }

  if (node_header->frozen) {
    return node_id;
  }

  const NodeState *node_state = frame.state(immediate_node_id);
  apply_node(node_id, frame, *node_header, *node_state);
  reconcile_children(
      node_id,
      frame,
      node_header->first_child_offset,
      node_header->child_count
  );
  return node_id;
}

void Runtime::reconcile_children(
    UINodeId parent_id,
    const Frame &frame,
    uint32_t first_child_offset,
    size_t child_count
) {
  ASTRA_PROFILE_N("im::Runtime::reconcile_children");
  auto *parent = m_document->node(parent_id);
  ASTRA_ENSURE(
      parent == nullptr,
      "ui::im::Runtime cannot reconcile children for an invalid parent"
  );

  std::vector<UINodeId> desired_children;
  desired_children.reserve(child_count);
  const uint64_t parent_generation = m_next_parent_generation++;

  const NodeId *children = frame.child_ids(first_child_offset);
  for (size_t index = 0u; index < child_count; ++index) {
    const UINodeId child_id = reconcile_node(frame, children[index]);
    desired_children.push_back(child_id);
    const WidgetId widget_id = child_id < m_node_widgets.size()
                                   ? m_node_widgets[child_id]
                                   : k_invalid_widget_id;
    if (widget_id != k_invalid_widget_id) {
      auto runtime_it = m_widget_nodes.find(widget_id);
      if (runtime_it != m_widget_nodes.end()) {
        runtime_it->second.last_parent_generation = parent_generation;
      }
    }
  }

  parent = m_document->node(parent_id);
  ASTRA_ENSURE(
      parent == nullptr,
      "ui::im::Runtime lost the parent while reconciling children"
  );

  for (size_t index = 0u; index < parent->children.size();) {
    const UINodeId child_id = parent->children[index];
    bool keep_child = false;
    const WidgetId widget_id = child_id < m_node_widgets.size()
                                   ? m_node_widgets[child_id]
                                   : k_invalid_widget_id;
    if (widget_id != k_invalid_widget_id) {
      const auto runtime_it = m_widget_nodes.find(widget_id);
      if (runtime_it != m_widget_nodes.end()) {
        keep_child =
            runtime_it->second.last_parent_generation == parent_generation;
      }
    }

    const bool is_live = child_id < m_live_nodes.size() && m_live_nodes[child_id];
    if (!keep_child && is_live) {
      destroy_runtime_subtree(child_id);
      parent = m_document->node(parent_id);
      ASTRA_ENSURE(
          parent == nullptr,
          "ui::im::Runtime lost the parent while pruning children"
      );
      continue;
    }

    ++index;
  }

  if (parent->children.size() == desired_children.size() &&
      std::equal(
          parent->children.begin(),
          parent->children.end(),
          desired_children.begin()
      )) {
    return;
  }

  m_document->clear_children(parent_id);
  for (const auto child_id : desired_children) {
    m_document->append_child(parent_id, child_id);
  }
}

void Runtime::apply_requests(const Frame &frame) {
  for (const auto &request : frame.focus_requests()) {
    const UINodeId node_id = node_id_for(request.widget_id);
    if (node_id != k_invalid_node_id) {
      m_document->request_focus(node_id);
    }
  }

  for (const auto &request : frame.pointer_capture_requests()) {
    const UINodeId node_id = node_id_for(request.widget_id);
    if (node_id != k_invalid_node_id) {
      m_document->request_pointer_capture(node_id, request.button);
    }
  }

  for (const auto &request : frame.pointer_capture_release_requests()) {
    const UINodeId node_id = node_id_for(request.widget_id);
    if (node_id != k_invalid_node_id) {
      m_document->release_pointer_capture(node_id, request.button);
    }
  }

  for (const auto &request : frame.text_selection_requests()) {
    const UINodeId node_id = node_id_for(request.widget_id);
    if (node_id != k_invalid_node_id) {
      m_document->set_text_selection(node_id, request.selection);
    }
  }

  for (const auto &request : frame.caret_requests()) {
    const UINodeId node_id = node_id_for(request.widget_id);
    if (node_id != k_invalid_node_id) {
      m_document->set_caret(node_id, request.index, request.active);
    }
  }

  for (const auto &request : frame.scroll_requests()) {
    const UINodeId node_id = node_id_for(request.widget_id);
    if (node_id != k_invalid_node_id) {
      m_document->set_scroll_offset(node_id, request.offset);
    }
  }

  for (const auto &request : frame.view_transform_requests()) {
    const UINodeId node_id = node_id_for(request.widget_id);
    if (node_id != k_invalid_node_id) {
      m_document->set_view_transform(node_id, request.transform);
    }
  }
}

} // namespace astralix::ui::im
