#pragma once

#include "dsl/core.hpp"

namespace astralix::ui::dsl {
namespace detail {

UINodeId create_view_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_text_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_image_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_render_image_view_node(
    UIDocument &document,
    const NodeSpec &spec
);
UINodeId create_pressable_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_icon_button_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_text_input_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_combobox_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_scroll_view_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_popover_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_splitter_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_button_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_checkbox_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_slider_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_select_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_segmented_control_node(
    UIDocument &document,
    const NodeSpec &spec
);
UINodeId create_chip_group_node(UIDocument &document, const NodeSpec &spec);
UINodeId create_line_chart_node(UIDocument &document, const NodeSpec &spec);

inline UINodeId create_node(UIDocument &document, const NodeSpec &spec) {
  switch (spec.kind) {
    case NodeKind::View:
      return create_view_node(document, spec);
    case NodeKind::Text:
      return create_text_node(document, spec);
    case NodeKind::Image:
      return create_image_node(document, spec);
    case NodeKind::RenderImageView:
      return create_render_image_view_node(document, spec);
    case NodeKind::Pressable:
      return create_pressable_node(document, spec);
    case NodeKind::IconButton:
      return create_icon_button_node(document, spec);
    case NodeKind::SegmentedControl:
      return create_segmented_control_node(document, spec);
    case NodeKind::ChipGroup:
      return create_chip_group_node(document, spec);
    case NodeKind::TextInput:
      return create_text_input_node(document, spec);
    case NodeKind::Combobox:
      return create_combobox_node(document, spec);
    case NodeKind::ScrollView:
      return create_scroll_view_node(document, spec);
    case NodeKind::Popover:
      return create_popover_node(document, spec);
    case NodeKind::Splitter:
      return create_splitter_node(document, spec);
    case NodeKind::Button:
      return create_button_node(document, spec);
    case NodeKind::Checkbox:
      return create_checkbox_node(document, spec);
    case NodeKind::Slider:
      return create_slider_node(document, spec);
    case NodeKind::Select:
      return create_select_node(document, spec);
    case NodeKind::LineChart:
      return create_line_chart_node(document, spec);
  }

  return k_invalid_node_id;
}

inline UINodeId materialize(
    UIDocument &document,
    std::optional<UINodeId> parent_id,
    const NodeSpec &spec
) {
  validate_spec(spec);

  const UINodeId node_id = create_node(document, spec);
  if (parent_id.has_value()) {
    document.append_child(*parent_id, node_id);
  }

  apply_properties(document, node_id, spec);
  for (const auto &child : spec.child_specs) {
    materialize(document, node_id, child);
  }

  return node_id;
}

} // namespace detail

inline UINodeId mount(UIDocument &document, const NodeSpec &spec) {
  const UINodeId root_id = detail::materialize(document, std::nullopt, spec);
  document.set_root(root_id);
  return root_id;
}

inline UINodeId append(UIDocument &document, UINodeId parent_id, const NodeSpec &spec) {
  ASTRA_ENSURE(
      parent_id == k_invalid_node_id,
      "ui::dsl append requires a valid parent"
  );
  return detail::materialize(document, parent_id, spec);
}

} // namespace astralix::ui::dsl
