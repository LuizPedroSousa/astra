#pragma once

#include "allocators/bump.hpp"
#include "assert.hpp"
#include "dsl.hpp"
#include "immediate/node.hpp"
#include "log.hpp"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace astralix::ui::im {

class Runtime;
struct VirtualListState {
  glm::vec2 scroll_offset = glm::vec2(0.0f);
  float viewport_height = 0.0f;
  float viewport_width = 0.0f;
};

struct FocusRequest {
  WidgetId widget_id = k_invalid_widget_id;
};

struct TextSelectionRequest {
  WidgetId widget_id = k_invalid_widget_id;
  UITextSelection selection;
};

struct CaretRequest {
  WidgetId widget_id = k_invalid_widget_id;
  size_t index = 0u;
  bool active = true;
};

struct ScrollRequest {
  WidgetId widget_id = k_invalid_widget_id;
  glm::vec2 offset = glm::vec2(0.0f);
};

class Frame;

class Children {
public:
  Children() = default;

  WidgetId resolve_widget_id(std::string_view local_name) const {
    ASTRA_ENSURE(
        m_frame == nullptr || m_scope_id == k_invalid_widget_id,
        "ui::im::Children cannot resolve ids without a valid scope"
    );
    return child_id(m_scope_id, local_name);
  }

  template <typename Key>
  Children item_scope(std::string_view local_name, const Key &key) const {
    ASTRA_ENSURE(
        m_frame == nullptr || m_scope_id == k_invalid_widget_id,
        "ui::im::Children cannot create keyed scopes without a valid scope"
    );
    return Children{
        m_frame,
        m_parent_id,
        keyed_id(m_scope_id, local_name, key),
    };
  }

  Children scope(std::string_view local_name) const {
    return Children{
        m_frame,
        m_parent_id,
        resolve_widget_id(local_name),
    };
  }

  class NodeHandle;

  NodeHandle view(std::string_view local_name);
  NodeHandle column(std::string_view local_name);
  NodeHandle row(std::string_view local_name);
  NodeHandle scroll_view(std::string_view local_name);
  NodeHandle spacer(std::string_view local_name);
  NodeHandle text(std::string_view local_name, std::string value);
  NodeHandle image(std::string_view local_name, ResourceDescriptorID texture_id);
  NodeHandle render_image_view(
      std::string_view local_name,
      RenderImageExportKey key
  );
  NodeHandle pressable(std::string_view local_name);
  NodeHandle text_input(
      std::string_view local_name,
      std::string value,
      std::string placeholder = {}
  );
  NodeHandle combobox(
      std::string_view local_name,
      std::string value,
      std::string placeholder = {}
  );
  NodeHandle checkbox(
      std::string_view local_name,
      std::string label,
      bool checked = false
  );
  NodeHandle slider(
      std::string_view local_name,
      float value,
      float min_value = 0.0f,
      float max_value = 1.0f
  );
  NodeHandle select(
      std::string_view local_name,
      std::vector<std::string> options,
      size_t selected_index = 0u,
      std::string placeholder = {}
  );
  NodeHandle segmented_control(
      std::string_view local_name,
      std::vector<std::string> options,
      size_t selected_index = 0u
  );
  NodeHandle chip_group(
      std::string_view local_name,
      std::vector<std::string> options,
      std::vector<bool> selected = {}
  );
  NodeHandle popover(std::string_view local_name);
  NodeHandle line_chart(std::string_view local_name);

  NodeHandle button(
      std::string_view local_name,
      std::string label,
      std::function<void()> on_click = {}
  );
  NodeHandle icon_button(
      std::string_view local_name,
      ResourceDescriptorID texture_id,
      std::function<void()> on_click = {}
  );

  template <typename HeightFn, typename DrawFn>
  NodeHandle virtual_list(
      std::string_view local_name,
      size_t item_count,
      HeightFn &&item_height,
      DrawFn &&draw_visible_items,
      float content_width = 0.0f,
      size_t overscan = 3u
  );

  NodeHandle lazy_section(
      std::string_view local_name,
      WidgetId scroll_parent_widget_id,
      float estimated_height,
      float overscan = 100.0f
  );

protected:
  friend class Frame;
  friend class Runtime;
  friend class NodeHandle;

  Children(
      Frame *frame,
      NodeId parent_id,
      WidgetId scope_id
  )
      : m_frame(frame), m_parent_id(parent_id), m_scope_id(scope_id) {}

  NodeHandle append_node(std::string_view local_name, dsl::NodeKind kind);
  NodeHandle append_scoped_node(WidgetId widget_id, dsl::NodeKind kind);

  Frame *m_frame = nullptr;
  NodeId m_parent_id = k_invalid_immediate_node_id;
  WidgetId m_scope_id = k_invalid_widget_id;
};

class Children::NodeHandle : public Children {
public:
  NodeHandle() = default;

  explicit operator bool() const {
    if (m_header_node == nullptr) {
      return false;
    }
    if (m_header_node->frozen) {
      return false;
    }
    if (bool_value_or(*m_header_node, NodeScalarField::Collapsed, false)) {
      return false;
    }
    return bool_value_or(*m_header_node, NodeScalarField::Visible, true);
  }

  WidgetId widget_id() const {
    return m_header_node != nullptr ? m_header_node->widget_id : k_invalid_widget_id;
  }

  NodeHandle &debug_name(std::string value) {
    if (m_header_node != nullptr) {
      ensure_text_payload().debug_name = std::move(value);
    }
    return *this;
  }

  template <typename Rule, typename... Rest>
  NodeHandle &style(Rule &&rule, Rest &&...rest);

  NodeHandle &visible(bool value) {
    if (m_header_node != nullptr) {
      set_bool_value(*m_header_node, NodeScalarField::Visible, value);
    }
    return *this;
  }

  NodeHandle &collapsed(bool value) {
    if (m_header_node != nullptr) {
      set_bool_value(*m_header_node, NodeScalarField::Collapsed, value);
    }
    return *this;
  }

  NodeHandle &frozen(bool value) {
    if (m_header_node != nullptr) {
      m_header_node->frozen = value;
    }
    return *this;
  }

  NodeHandle &enabled(bool value) {
    if (m_header_node != nullptr) {
      set_bool_value(*m_header_node, NodeScalarField::Enabled, value);
    }
    return *this;
  }

  NodeHandle &focusable(bool value) {
    if (m_header_node != nullptr) {
      set_bool_value(*m_header_node, NodeScalarField::Focusable, value);
    }
    return *this;
  }

  NodeHandle &read_only(bool value) {
    if (m_header_node != nullptr) {
      set_bool_value(*m_header_node, NodeScalarField::ReadOnly, value);
    }
    return *this;
  }

  NodeHandle &select_all_on_focus(bool value) {
    if (m_header_node != nullptr) {
      set_bool_value(*m_header_node, NodeScalarField::SelectAllOnFocus, value);
    }
    return *this;
  }

  NodeHandle &checked(bool value) {
    if (m_header_node != nullptr) {
      set_bool_value(*m_header_node, NodeScalarField::Checked, value);
    }
    return *this;
  }

  NodeHandle &slider_value(float value) {
    if (m_header_node != nullptr) {
      set_value(
          NodeScalarField::SliderValue,
          m_state_node->slider_value,
          value
      );
    }
    return *this;
  }

  NodeHandle &range(float min_value, float max_value, float step = 0.1f) {
    if (m_header_node != nullptr) {
      set_value(
          NodeScalarField::SliderMin,
          m_state_node->slider_min_value,
          min_value
      );
      set_value(
          NodeScalarField::SliderMax,
          m_state_node->slider_max_value,
          max_value
      );
      set_value(
          NodeScalarField::SliderStep,
          m_state_node->slider_step_value,
          step
      );
    }
    return *this;
  }

  NodeHandle &step(float value) {
    if (m_header_node != nullptr) {
      set_value(
          NodeScalarField::SliderStep,
          m_state_node->slider_step_value,
          value
      );
    }
    return *this;
  }

  NodeHandle &selected_index(size_t value) {
    if (m_header_node != nullptr) {
      set_value(
          NodeScalarField::SelectedIndex,
          m_state_node->selected_index_value,
          value
      );
    }
    return *this;
  }

  NodeHandle &highlighted_index(size_t value) {
    if (m_header_node != nullptr) {
      set_value(
          NodeScalarField::HighlightedIndex,
          m_state_node->highlighted_index_value,
          value
      );
    }
    return *this;
  }

  NodeHandle &options(std::vector<std::string> values) {
    if (m_header_node != nullptr) {
      ensure_option_payload().option_values = std::move(values);
    }
    return *this;
  }

  NodeHandle &accent_colors(std::vector<glm::vec4> values) {
    if (m_header_node != nullptr) {
      ensure_option_payload().item_accent_colors = std::move(values);
    }
    return *this;
  }

  NodeHandle &chip_selected(std::vector<bool> values) {
    if (m_header_node != nullptr) {
      ensure_option_payload().chip_selected_values = std::move(values);
    }
    return *this;
  }

  NodeHandle &text_value(std::string value) {
    if (m_header_node != nullptr) {
      ensure_text_payload().text = std::move(value);
    }
    return *this;
  }

  NodeHandle &placeholder(std::string value) {
    if (m_header_node != nullptr) {
      ensure_text_payload().placeholder = std::move(value);
    }
    return *this;
  }

  NodeHandle &autocomplete_text(std::string value) {
    if (m_header_node != nullptr) {
      ensure_text_payload().autocomplete_text = std::move(value);
    }
    return *this;
  }

  NodeHandle &texture(ResourceDescriptorID texture_id) {
    if (m_header_node != nullptr) {
      ensure_text_payload().texture_id = std::move(texture_id);
    }
    return *this;
  }

  NodeHandle &render_image_key(RenderImageExportKey key) {
    if (m_state_node != nullptr) {
      m_state_node->render_image_key = key;
    }
    return *this;
  }

  NodeHandle &combobox_open(bool value) {
    if (m_header_node != nullptr) {
      set_bool_value(*m_header_node, NodeScalarField::ComboboxOpen, value);
    }
    return *this;
  }

  NodeHandle &select_open(bool value) {
    if (m_header_node != nullptr) {
      set_bool_value(*m_header_node, NodeScalarField::SelectOpen, value);
    }
    return *this;
  }

  NodeHandle &open_on_arrow_keys(bool value) {
    if (m_header_node != nullptr) {
      set_bool_value(
          *m_header_node,
          NodeScalarField::ComboboxOpenOnArrowKeys,
          value
      );
    }
    return *this;
  }

  NodeHandle &popover(const PopoverState &state) {
    if (m_header_node != nullptr) {
      ensure_popover_payload().state = state;
    }
    return *this;
  }

  NodeHandle &line_chart_grid(size_t line_count, glm::vec4 color) {
    if (m_header_node != nullptr) {
      auto &payload = ensure_line_chart_payload();
      payload.grid_line_count = line_count;
      payload.grid_color = color;
    }
    return *this;
  }

  NodeHandle &line_chart_series(std::vector<UILineChartSeries> series) {
    if (m_header_node != nullptr) {
      auto &payload = ensure_line_chart_payload();
      payload.has_series = true;
      payload.series = std::move(series);
    }
    return *this;
  }

  NodeHandle &line_chart_auto_range(bool value) {
    if (m_header_node != nullptr) {
      ensure_line_chart_payload().auto_range = value;
    }
    return *this;
  }

  NodeHandle &line_chart_range(float y_min, float y_max) {
    if (m_header_node != nullptr) {
      auto &payload = ensure_line_chart_payload();
      payload.y_min = y_min;
      payload.y_max = y_max;
    }
    return *this;
  }

  NodeHandle &on_hover(std::function<void()> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_hover_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &on_press(std::function<void()> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_press_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &on_release(std::function<void()> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_release_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &on_click(std::function<void()> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_click_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &
  on_secondary_click(std::function<void(const UIPointerButtonEvent &)> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_secondary_click_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &on_focus(std::function<void()> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_focus_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &on_blur(std::function<void()> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_blur_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &
  on_key_input(std::function<void(const UIKeyInputEvent &)> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_key_input_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &on_character_input(
      std::function<void(const UICharacterInputEvent &)> callback
  ) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_character_input_callback =
          std::move(callback);
    }
    return *this;
  }

  NodeHandle &on_mouse_wheel(
      std::function<void(const UIMouseWheelInputEvent &)> callback
  ) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_mouse_wheel_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &on_change(std::function<void(const std::string &)> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_change_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &on_submit(std::function<void(const std::string &)> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_submit_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &on_toggle(std::function<void(bool)> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_toggle_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &on_value_change(std::function<void(float)> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_value_change_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &
  on_select(std::function<void(size_t, const std::string &)> callback) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_select_callback = std::move(callback);
    }
    return *this;
  }

  NodeHandle &on_chip_toggle(
      std::function<void(size_t, const std::string &, bool)> callback
  ) {
    if (m_header_node != nullptr) {
      ensure_callback_payload().on_chip_toggle_callback = std::move(callback);
    }
    return *this;
  }

private:
  friend class Children;

  NodeHandle(Frame *frame, NodeId node_id);

  template <typename T>
  void set_value(
      NodeScalarField field,
      T &slot,
      T value
  ) {
    m_header_node->scalar_mask |= node_scalar_bit(field);
    slot = std::move(value);
  }

  static void set_bool_value(
      NodeHeader &node,
      NodeScalarField field,
      bool value
  ) {
    const NodeScalarMask bit = node_scalar_bit(field);
    node.scalar_mask |= bit;
    if (value) {
      node.bool_mask |= bit;
    } else {
      node.bool_mask &= static_cast<NodeScalarMask>(~bit);
    }
  }

  NodeTextPayload &ensure_text_payload() const;
  NodeOptionPayload &ensure_option_payload() const;
  NodeLineChartPayload &ensure_line_chart_payload() const;
  NodePopoverPayload &ensure_popover_payload() const;
  NodeCallbackPayload &ensure_callback_payload() const;

  NodeId m_node_id = k_invalid_immediate_node_id;
  NodeHeader *m_header_node = nullptr;
  NodeState *m_state_node = nullptr;
};

class Frame : public Children {
public:
  Frame(
      Runtime *runtime,
      BumpAllocator *allocator,
      WidgetId root_scope_id,
      size_t previous_node_count = 0u
  );

  struct LazySectionEntry {
    WidgetId section_widget_id;
    float height;
  };

  Runtime *runtime() const { return m_runtime; }
  VirtualListState virtual_list_state(WidgetId widget_id) const;
  float measured_node_height(WidgetId widget_id) const;
  std::vector<LazySectionEntry> &lazy_section_registry(WidgetId scroll_parent_id);
  NodeId root_node_id() const { return m_root_node_id; }
  size_t node_count() const { return m_header_nodes.size(); }
  const NodeHeader *header(NodeId node_id) const {
    return node_id < m_header_nodes.size() ? &m_header_nodes[node_id] : nullptr;
  }
  NodeHeader *header(NodeId node_id) {
    return node_id < m_header_nodes.size() ? &m_header_nodes[node_id] : nullptr;
  }
  const NodeState *state(NodeId node_id) const {
    return node_id < m_state_nodes.size() ? &m_state_nodes[node_id] : nullptr;
  }
  NodeState *state(NodeId node_id) {
    return node_id < m_state_nodes.size() ? &m_state_nodes[node_id] : nullptr;
  }
  const NodeTextPayload &text_payload(const NodeState &state) const;
  const NodeOptionPayload &option_payload(const NodeState &state) const;
  const NodeLineChartPayload &line_chart_payload(const NodeState &state) const;
  const NodePopoverPayload &popover_payload(const NodeState &state) const;
  const NodeCallbackPayload &callback_payload(const NodeState &state) const;
  const dsl::StyleStep &style_step(size_t index) const {
    return m_style_steps[index];
  }
  const dsl::StyleStepStorage &style_steps() const { return m_style_steps; }
  const NodeId *child_ids(uint32_t offset) const { return &m_child_ids[offset]; }
  void flatten_children();

  void request_focus(WidgetId widget_id) {
    m_focus_requests.push_back(FocusRequest{widget_id});
  }

  void set_text_selection(WidgetId widget_id, UITextSelection selection) {
    m_text_selection_requests.push_back(TextSelectionRequest{
        .widget_id = widget_id,
        .selection = selection,
    });
  }

  void set_caret(WidgetId widget_id, size_t index, bool active = true) {
    m_caret_requests.push_back(CaretRequest{
        .widget_id = widget_id,
        .index = index,
        .active = active,
    });
  }

  void set_scroll_offset(WidgetId widget_id, glm::vec2 offset) {
    m_scroll_requests.push_back(ScrollRequest{
        .widget_id = widget_id,
        .offset = offset,
    });
  }

  const std::vector<FocusRequest> &focus_requests() const {
    return m_focus_requests;
  }
  const std::vector<TextSelectionRequest> &text_selection_requests() const {
    return m_text_selection_requests;
  }
  const std::vector<CaretRequest> &caret_requests() const {
    return m_caret_requests;
  }
  const std::vector<ScrollRequest> &scroll_requests() const {
    return m_scroll_requests;
  }

private:
  friend class Children;
  friend class Children::NodeHandle;

  NodeId create_node(dsl::NodeKind kind, WidgetId widget_id);
  void append_child(NodeId parent_id, NodeId child_id);
  void append_style(NodeId node_id, dsl::StyleBuilder builder);
  NodeTextPayload &ensure_text_payload(NodeId node_id);
  NodeOptionPayload &ensure_option_payload(NodeId node_id);
  NodeLineChartPayload &ensure_line_chart_payload(NodeId node_id);
  NodePopoverPayload &ensure_popover_payload(NodeId node_id);
  NodeCallbackPayload &ensure_callback_payload(NodeId node_id);

  template <typename Payload>
  static const Payload &payload_or_empty(
      const BumpVector<Payload> &payloads,
      PayloadIndex index
  ) {
    static const Payload k_empty{};
    return index != k_invalid_immediate_payload_index &&
                   index < payloads.size()
               ? payloads[index]
               : k_empty;
  }

  template <typename Payload>
  static Payload &ensure_payload(
      BumpVector<Payload> &payloads,
      PayloadIndex &index
  ) {
    if (index == k_invalid_immediate_payload_index) {
      index = static_cast<PayloadIndex>(payloads.size());
      payloads.emplace_back();
    }
    return payloads[index];
  }

  Runtime *m_runtime = nullptr;
  BumpAllocator *m_allocator = nullptr;
  BumpVector<NodeHeader> m_header_nodes;
  BumpVector<NodeState> m_state_nodes;
  BumpVector<NodeLinkChain> m_link_chains;
  BumpVector<NodeId> m_child_ids;
  dsl::StyleStepStorage m_style_steps;
  BumpVector<NodeTextPayload> m_text_payloads;
  BumpVector<NodeOptionPayload> m_option_payloads;
  BumpVector<NodeLineChartPayload> m_line_chart_payloads;
  BumpVector<NodePopoverPayload> m_popover_payloads;
  BumpVector<NodeCallbackPayload> m_callback_payloads;
  NodeId m_root_node_id = k_invalid_immediate_node_id;
  std::vector<FocusRequest> m_focus_requests;
  std::vector<TextSelectionRequest> m_text_selection_requests;
  std::vector<CaretRequest> m_caret_requests;
  std::vector<ScrollRequest> m_scroll_requests;
  std::unordered_map<WidgetId, std::vector<LazySectionEntry>> m_lazy_section_registries;
};

template <typename Rule, typename... Rest>
inline Children::NodeHandle &
Children::NodeHandle::style(Rule &&rule, Rest &&...rest) {
  if (m_header_node != nullptr) {
    m_frame->append_style(m_node_id, std::forward<Rule>(rule));
  }
  if constexpr (sizeof...(rest) > 0) {
    style(std::forward<Rest>(rest)...);
  }
  return *this;
}

inline NodeTextPayload &Children::NodeHandle::ensure_text_payload() const {
  return m_frame->ensure_text_payload(m_node_id);
}

inline NodeOptionPayload &Children::NodeHandle::ensure_option_payload() const {
  return m_frame->ensure_option_payload(m_node_id);
}

inline NodeLineChartPayload &
Children::NodeHandle::ensure_line_chart_payload() const {
  return m_frame->ensure_line_chart_payload(m_node_id);
}

inline NodePopoverPayload &Children::NodeHandle::ensure_popover_payload() const {
  return m_frame->ensure_popover_payload(m_node_id);
}

inline NodeCallbackPayload &
Children::NodeHandle::ensure_callback_payload() const {
  return m_frame->ensure_callback_payload(m_node_id);
}

inline Children::NodeHandle::NodeHandle(Frame *frame, NodeId node_id)
    : Children(frame, node_id, k_invalid_widget_id), m_node_id(node_id),
      m_header_node(frame != nullptr ? frame->header(node_id) : nullptr),
      m_state_node(frame != nullptr ? frame->state(node_id) : nullptr) {
  if (m_header_node != nullptr) {
    m_scope_id = m_header_node->widget_id;
  }
}

inline NodeId Frame::create_node(dsl::NodeKind kind, WidgetId widget_id) {
  const NodeId node_id = static_cast<NodeId>(m_header_nodes.size());
  m_header_nodes.emplace_back();
  m_state_nodes.emplace_back();
  m_link_chains.emplace_back();
  NodeHeader &header = m_header_nodes.back();
  header.kind = kind;
  header.widget_id = widget_id;
  return node_id;
}

inline const NodeTextPayload &Frame::text_payload(const NodeState &state) const {
  return payload_or_empty(m_text_payloads, state.text_payload_index);
}

inline const NodeOptionPayload &
Frame::option_payload(const NodeState &state) const {
  return payload_or_empty(m_option_payloads, state.option_payload_index);
}

inline const NodeLineChartPayload &
Frame::line_chart_payload(const NodeState &state) const {
  return payload_or_empty(
      m_line_chart_payloads,
      state.line_chart_payload_index
  );
}

inline const NodePopoverPayload &
Frame::popover_payload(const NodeState &state) const {
  return payload_or_empty(m_popover_payloads, state.popover_payload_index);
}

inline const NodeCallbackPayload &
Frame::callback_payload(const NodeState &state) const {
  return payload_or_empty(m_callback_payloads, state.callback_payload_index);
}

inline void Frame::append_style(NodeId node_id, dsl::StyleBuilder builder) {
  if (!builder) {
    return;
  }

  NodeHeader &header = m_header_nodes[node_id];
  const uint32_t next_step_count =
      static_cast<uint32_t>(builder.step_count());
  if (header.style_step_count == 0u) {
    header.first_style_step = static_cast<uint32_t>(m_style_steps.size());
    std::move(builder).append_steps_to(m_style_steps);
    header.style_step_count = next_step_count;
    return;
  }

  const uint32_t span_end = header.first_style_step + header.style_step_count;
  if (span_end == m_style_steps.size()) {
    std::move(builder).append_steps_to(m_style_steps);
    header.style_step_count += next_step_count;
    return;
  }

  const uint32_t existing_count = header.style_step_count;
  const uint32_t relocated_first = static_cast<uint32_t>(m_style_steps.size());
  dsl::reserve_style_steps(
      m_style_steps,
      m_style_steps.size() + existing_count + next_step_count
  );
  for (uint32_t index = 0u; index < existing_count; ++index) {
    m_style_steps.emplace_back(m_style_steps[header.first_style_step + index]);
  }
  std::move(builder).append_steps_to(m_style_steps);
  header.first_style_step = relocated_first;
  header.style_step_count = existing_count + next_step_count;
}

inline NodeTextPayload &Frame::ensure_text_payload(NodeId node_id) {
  return ensure_payload(
      m_text_payloads,
      m_state_nodes[node_id].text_payload_index
  );
}

inline NodeOptionPayload &Frame::ensure_option_payload(NodeId node_id) {
  return ensure_payload(
      m_option_payloads,
      m_state_nodes[node_id].option_payload_index
  );
}

inline NodeLineChartPayload &Frame::ensure_line_chart_payload(NodeId node_id) {
  return ensure_payload(
      m_line_chart_payloads,
      m_state_nodes[node_id].line_chart_payload_index
  );
}

inline NodePopoverPayload &Frame::ensure_popover_payload(NodeId node_id) {
  return ensure_payload(
      m_popover_payloads,
      m_state_nodes[node_id].popover_payload_index
  );
}

inline NodeCallbackPayload &Frame::ensure_callback_payload(NodeId node_id) {
  return ensure_payload(
      m_callback_payloads,
      m_state_nodes[node_id].callback_payload_index
  );
}

inline void Frame::append_child(NodeId parent_id, NodeId child_id) {
  ASTRA_ENSURE(
      parent_id == k_invalid_immediate_node_id || child_id >= m_header_nodes.size(),
      "ui::im::Frame cannot link an invalid child"
  );

  NodeLinkChain &parent_links = m_link_chains[parent_id];
  if (parent_links.first_child_id == k_invalid_immediate_node_id) {
    parent_links.first_child_id = child_id;
    parent_links.last_child_id = child_id;
  } else {
    m_link_chains[parent_links.last_child_id].next_sibling_id = child_id;
    parent_links.last_child_id = child_id;
  }
  ++m_header_nodes[parent_id].child_count;
}

inline Children::NodeHandle
Children::append_scoped_node(WidgetId widget_id, dsl::NodeKind kind) {
  ASTRA_ENSURE(
      m_frame == nullptr || m_parent_id == k_invalid_immediate_node_id ||
          widget_id == k_invalid_widget_id,
      "ui::im::Children cannot append a node without a valid destination"
  );
  const NodeId node_id = m_frame->create_node(kind, widget_id);
  m_frame->append_child(m_parent_id, node_id);
  return NodeHandle{m_frame, node_id};
}

inline Children::NodeHandle
Children::append_node(std::string_view local_name, dsl::NodeKind kind) {
  return append_scoped_node(resolve_widget_id(local_name), kind);
}

inline Children::NodeHandle Children::view(std::string_view local_name) {
  return append_node(local_name, dsl::NodeKind::View);
}

inline Children::NodeHandle Children::column(std::string_view local_name) {
  auto node = view(local_name);
  node.style(dsl::styles::column());
  return node;
}

inline Children::NodeHandle Children::row(std::string_view local_name) {
  auto node = view(local_name);
  node.style(dsl::styles::row());
  return node;
}

inline Children::NodeHandle Children::scroll_view(std::string_view local_name) {
  return append_node(local_name, dsl::NodeKind::ScrollView);
}

inline Children::NodeHandle Children::spacer(std::string_view local_name) {
  auto node = view(local_name);
  node.style(dsl::styles::flex(1.0f));
  return node;
}

inline Children::NodeHandle
Children::text(std::string_view local_name, std::string value) {
  auto node = append_node(local_name, dsl::NodeKind::Text);
  node.text_value(std::move(value));
  return node;
}

inline Children::NodeHandle Children::image(
    std::string_view local_name,
    ResourceDescriptorID texture_id
) {
  auto node = append_node(local_name, dsl::NodeKind::Image);
  node.texture(std::move(texture_id));
  return node;
}

inline Children::NodeHandle Children::render_image_view(
    std::string_view local_name,
    RenderImageExportKey key
) {
  auto node = append_node(local_name, dsl::NodeKind::RenderImageView);
  node.render_image_key(key);
  return node;
}

inline Children::NodeHandle Children::pressable(std::string_view local_name) {
  return append_node(local_name, dsl::NodeKind::Pressable);
}

inline Children::NodeHandle Children::text_input(
    std::string_view local_name,
    std::string value,
    std::string placeholder
) {
  auto node = append_node(local_name, dsl::NodeKind::TextInput);
  node.text_value(std::move(value));
  node.placeholder(std::move(placeholder));
  return node;
}

inline Children::NodeHandle Children::combobox(
    std::string_view local_name,
    std::string value,
    std::string placeholder
) {
  auto node = append_node(local_name, dsl::NodeKind::Combobox);
  node.text_value(std::move(value));
  node.placeholder(std::move(placeholder));
  return node;
}

inline Children::NodeHandle Children::checkbox(
    std::string_view local_name,
    std::string label,
    bool checked
) {
  auto node = append_node(local_name, dsl::NodeKind::Checkbox);
  node.text_value(std::move(label));
  node.checked(checked);
  return node;
}

inline Children::NodeHandle Children::slider(
    std::string_view local_name,
    float value,
    float min_value,
    float max_value
) {
  auto node = append_node(local_name, dsl::NodeKind::Slider);
  node.slider_value(value);
  node.range(min_value, max_value);
  return node;
}

inline Children::NodeHandle Children::select(
    std::string_view local_name,
    std::vector<std::string> options,
    size_t selected_index,
    std::string placeholder
) {
  auto node = append_node(local_name, dsl::NodeKind::Select);
  node.options(std::move(options));
  node.selected_index(selected_index);
  node.placeholder(std::move(placeholder));
  return node;
}

inline Children::NodeHandle Children::segmented_control(
    std::string_view local_name,
    std::vector<std::string> options,
    size_t selected_index
) {
  auto node = append_node(local_name, dsl::NodeKind::SegmentedControl);
  node.options(std::move(options));
  node.selected_index(selected_index);
  return node;
}

inline Children::NodeHandle Children::chip_group(
    std::string_view local_name,
    std::vector<std::string> options,
    std::vector<bool> selected
) {
  auto node = append_node(local_name, dsl::NodeKind::ChipGroup);
  node.options(std::move(options));
  node.chip_selected(std::move(selected));
  return node;
}

inline Children::NodeHandle Children::popover(std::string_view local_name) {
  return append_node(local_name, dsl::NodeKind::Popover);
}

inline Children::NodeHandle Children::line_chart(std::string_view local_name) {
  return append_node(local_name, dsl::NodeKind::LineChart);
}

inline Children::NodeHandle Children::button(
    std::string_view local_name,
    std::string label,
    std::function<void()> on_click
) {
  auto root = pressable(local_name)
                  .on_click(std::move(on_click))
                  .style(
                      dsl::styles::items_center().justify_center().cursor_pointer()
                  );
  root.text("label", std::move(label));
  return root;
}

inline Children::NodeHandle Children::icon_button(
    std::string_view local_name,
    ResourceDescriptorID texture_id,
    std::function<void()> on_click
) {
  auto root = pressable(local_name)
                  .on_click(std::move(on_click))
                  .style(
                      dsl::styles::items_center().justify_center().cursor_pointer()
                  );
  root.image("icon", std::move(texture_id))
      .style(
          dsl::styles::width(UILength::pixels(16.0f))
              .height(UILength::pixels(16.0f))
      );
  return root;
}

template <typename HeightFn, typename DrawFn>
Children::NodeHandle Children::virtual_list(
    std::string_view local_name,
    size_t item_count,
    HeightFn &&item_height,
    DrawFn &&draw_visible_items,
    float content_width,
    size_t overscan
) {
  auto scroll = scroll_view(local_name);
  auto content = scroll.column("content");
  content.style(
      dsl::styles::gap(0.0f),
      dsl::styles::width(
          content_width > 0.0f ? UILength::pixels(content_width)
                               : UILength::percent(1.0f)
      )
  );

  size_t start = 0u;
  size_t end = item_count == 0u ? 0u : item_count - 1u;
  float top_height = 0.0f;
  float bottom_height = 0.0f;
  if (item_count > 0u && m_frame != nullptr && m_frame->runtime() != nullptr) {
    const auto state = m_frame->virtual_list_state(scroll.widget_id());
    const float viewport_height = state.viewport_height;
    if (viewport_height > 0.0f) {
      const float scroll_top = std::max(0.0f, state.scroll_offset.y);
      const float scroll_bottom = scroll_top + viewport_height;
      float total_height = 0.0f;
      size_t first_visible = item_count;
      size_t last_visible = item_count;
      float first_visible_top = 0.0f;

      for (size_t index = 0u; index < item_count; ++index) {
        const float height = std::max(0.0f, static_cast<float>(item_height(index)));
        const float item_top = total_height;
        const float item_bottom = item_top + height;

        if (first_visible == item_count && item_bottom > scroll_top) {
          first_visible = index;
          first_visible_top = item_top;
        }

        if (first_visible != item_count && item_top < scroll_bottom) {
          last_visible = index;
        }

        total_height = item_bottom;
      }

      if (first_visible == item_count) {
        start = item_count - 1u;
        end = item_count - 1u;
        top_height = 0.0f;
        for (size_t index = 0u; index + 1u < item_count; ++index) {
          top_height +=
              std::max(0.0f, static_cast<float>(item_height(index)));
        }
      } else {
        if (last_visible == item_count) {
          last_visible = first_visible;
        }
        start = first_visible > overscan ? first_visible - overscan : 0u;
        end = std::min(item_count - 1u, last_visible + overscan);

        top_height = first_visible_top;
        for (size_t index = start; index < first_visible; ++index) {
          top_height -=
              std::max(0.0f, static_cast<float>(item_height(index)));
        }

        bottom_height = total_height - top_height;
        for (size_t index = start; index <= end; ++index) {
          bottom_height -=
              std::max(0.0f, static_cast<float>(item_height(index)));
        }
      }
    } else {
      end = std::min(item_count - 1u, std::max<size_t>(1u, overscan * 2u + 1u) - 1u);
      for (size_t index = end + 1u; index < item_count; ++index) {
        bottom_height +=
            std::max(0.0f, static_cast<float>(item_height(index)));
      }
    }
  } else if (item_count > 0u) {
    for (size_t index = end + 1u; index < item_count; ++index) {
      bottom_height += std::max(0.0f, static_cast<float>(item_height(index)));
    }
  }

  if (top_height > 0.0f) {
    content.view("top_spacer")
        .style(
            dsl::styles::height(UILength::pixels(top_height)),
            dsl::styles::shrink(0.0f)
        );
  }

  if (item_count > 0u && start <= end) {
    auto visible_scope = content.scope("visible");
    draw_visible_items(visible_scope, start, end);
  }

  if (bottom_height > 0.0f) {
    content.view("bottom_spacer")
        .style(
            dsl::styles::height(UILength::pixels(bottom_height)),
            dsl::styles::shrink(0.0f)
        );
  }

  return scroll;
}

inline Children::NodeHandle Children::lazy_section(
    std::string_view local_name,
    WidgetId scroll_parent_widget_id,
    float estimated_height,
    float overscan
) {
  auto section = view(local_name);

  if (m_frame == nullptr) {
    return section;
  }

  const WidgetId section_widget_id = section.widget_id();
  const float measured = m_frame->measured_node_height(section_widget_id);
  const float section_height =
      measured > 0.0f ? measured : std::max(0.0f, estimated_height);

  auto &registry = m_frame->lazy_section_registry(scroll_parent_widget_id);
  float cumulative_offset = 0.0f;
  for (const auto &entry : registry) {
    cumulative_offset += entry.height;
  }
  registry.push_back({section_widget_id, section_height});

  const auto scroll_state =
      m_frame->virtual_list_state(scroll_parent_widget_id);
  const float viewport_height = scroll_state.viewport_height;

  if (viewport_height > 0.0f) {
    const float scroll_top = std::max(0.0f, scroll_state.scroll_offset.y);
    const float scroll_bottom = scroll_top + viewport_height;
    const float section_top = cumulative_offset;
    const float section_bottom = section_top + section_height;
    const bool intersects = section_bottom > (scroll_top - overscan) &&
                            section_top < (scroll_bottom + overscan);

    if (!intersects) {
      section.style(
          dsl::styles::height(UILength::pixels(section_height)),
          dsl::styles::shrink(0.0f)
      );
      section.collapsed(true);
    }
  }

  return section;
}

} // namespace astralix::ui::im
