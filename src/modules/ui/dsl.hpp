#pragma once

#include "assert.hpp"
#include "document.hpp"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace astralix::ui::dsl {

  using StyleRule = std::function<void(UIStyle&)>;
  using StateStyleRule = std::function<void(UIStateStyle&)>;

  class StateStyleBuilder {
  public:
    StateStyleBuilder& background(glm::vec4 color) {
      return add(
        [color](UIStateStyle& style) { style.background_color = color; }
      );
    }

    StateStyleBuilder& border(float width, glm::vec4 color) {
      return add([width, color](UIStateStyle& style) {
        style.border_width = width;
        style.border_color = color;
        });
    }

    StateStyleBuilder& radius(float value) {
      return add([value](UIStateStyle& style) { style.border_radius = value; });
    }

    StateStyleBuilder& opacity(float value) {
      return add([value](UIStateStyle& style) { style.opacity = value; });
    }

    StateStyleBuilder& tint(glm::vec4 value) {
      return add([value](UIStateStyle& style) { style.tint = value; });
    }

    StateStyleBuilder& text_color(glm::vec4 color) {
      return add([color](UIStateStyle& style) { style.text_color = color; });
    }

    StateStyleBuilder& raw(StateStyleRule rule) { return add(std::move(rule)); }

    operator StateStyleRule() const {
      const auto rules = m_rules;
      return [rules](UIStateStyle& style) {
        for (const auto& rule : rules) {
          if (rule) {
            rule(style);
          }
        }
        };
    }

  private:
    StateStyleBuilder& add(StateStyleRule rule) {
      if (rule) {
        m_rules.push_back(std::move(rule));
      }

      return *this;
    }

    std::vector<StateStyleRule> m_rules;
  };

  class StyleBuilder {
  public:
    StyleBuilder& row() {
      return add(
        [](UIStyle& style) { style.flex_direction = FlexDirection::Row; }
      );
    }

    StyleBuilder& column() {
      return add(
        [](UIStyle& style) { style.flex_direction = FlexDirection::Column; }
      );
    }

    StyleBuilder& items_start() {
      return add([](UIStyle& style) { style.align_items = AlignItems::Start; });
    }

    StyleBuilder& items_end() {
      return add([](UIStyle& style) { style.align_items = AlignItems::End; });
    }

    StyleBuilder& items_center() {
      return add([](UIStyle& style) { style.align_items = AlignItems::Center; });
    }

    StyleBuilder& justify_start() {
      return add(
        [](UIStyle& style) { style.justify_content = JustifyContent::Start; }
      );
    }

    StyleBuilder& justify_end() {
      return add(
        [](UIStyle& style) { style.justify_content = JustifyContent::End; }
      );
    }

    StyleBuilder& justify_center() {
      return add(
        [](UIStyle& style) { style.justify_content = JustifyContent::Center; }
      );
    }

    StyleBuilder& gap(float value) {
      return add([value](UIStyle& style) { style.gap = value; });
    }

    StyleBuilder& grow(float value = 1.0f) {
      return add([value](UIStyle& style) { style.flex_grow = value; });
    }

    StyleBuilder& shrink(float value = 1.0f) {
      return add([value](UIStyle& style) { style.flex_shrink = value; });
    }

    StyleBuilder& basis(UILength value) {
      return add([value](UIStyle& style) { style.flex_basis = value; });
    }

    StyleBuilder& flex(float value = 1.0f) {
      return add([value](UIStyle& style) {
        style.flex_grow = value;
        style.flex_shrink = value > 0.0f ? 1.0f : 0.0f;
        style.flex_basis =
          value > 0.0f ? UILength::pixels(0.0f) : UILength::auto_value();
        });
    }

    StyleBuilder& width(UILength value) {
      return add([value](UIStyle& style) { style.width = value; });
    }

    StyleBuilder& height(UILength value) {
      return add([value](UIStyle& style) { style.height = value; });
    }

    StyleBuilder& left(UILength value) {
      return add([value](UIStyle& style) { style.left = value; });
    }

    StyleBuilder& top(UILength value) {
      return add([value](UIStyle& style) { style.top = value; });
    }

    StyleBuilder& right(UILength value) {
      return add([value](UIStyle& style) { style.right = value; });
    }

    StyleBuilder& bottom(UILength value) {
      return add([value](UIStyle& style) { style.bottom = value; });
    }

    StyleBuilder& fill_x() { return width(UILength::percent(1.0f)); }

    StyleBuilder& fill_y() { return height(UILength::percent(1.0f)); }

    StyleBuilder& fill() { return fill_x().fill_y(); }

    StyleBuilder& absolute() {
      return add(
        [](UIStyle& style) { style.position_type = PositionType::Absolute; }
      );
    }

    StyleBuilder& relative() {
      return add(
        [](UIStyle& style) { style.position_type = PositionType::Relative; }
      );
    }

    StyleBuilder& padding(float value) {
      return add(
        [value](UIStyle& style) { style.padding = UIEdges::all(value); }
      );
    }

    StyleBuilder& padding_xy(float horizontal, float vertical) {
      return add([horizontal, vertical](UIStyle& style) {
        style.padding = UIEdges::symmetric(horizontal, vertical);
        });
    }

    StyleBuilder& padding_y(float vertical) {
      return add([vertical](UIStyle& style) {
        style.padding = UIEdges::symmetric(style.padding.horizontal(), vertical);
        });
    }

    StyleBuilder& padding_x(float horizontal) {
      return add([horizontal](UIStyle& style) {
        style.padding = UIEdges::symmetric(horizontal, style.padding.vertical());
        });
    }

    StyleBuilder& background(glm::vec4 color) {
      return add([color](UIStyle& style) { style.background_color = color; });
    }

    StyleBuilder& border(float width, glm::vec4 color) {
      return add([width, color](UIStyle& style) {
        style.border_width = width;
        style.border_color = color;
        });
    }

    StyleBuilder& radius(float value) {
      return add([value](UIStyle& style) { style.border_radius = value; });
    }

    StyleBuilder& text_color(glm::vec4 color) {
      return add([color](UIStyle& style) { style.text_color = color; });
    }

    StyleBuilder& font_size(float value) {
      return add([value](UIStyle& style) { style.font_size = value; });
    }

    StyleBuilder& overflow_hidden() {
      return add([](UIStyle& style) { style.overflow = Overflow::Hidden; });
    }

    StyleBuilder& scroll_both() {
      return add([](UIStyle& style) { style.scroll_mode = ScrollMode::Both; });
    }

    StyleBuilder& scrollbar_auto() {
      return add([](UIStyle& style) {
        style.scrollbar_visibility = ScrollbarVisibility::Auto;
        });
    }

    StyleBuilder& resizable_all() {
      return add([](UIStyle& style) {
        style.resize_mode = ResizeMode::Both;
        style.resize_edges = k_resize_edge_all;
        });
    }

    StyleBuilder& draggable() {
      return add([](UIStyle& style) { style.draggable = true; });
    }

    StyleBuilder& drag_handle() {
      return add([](UIStyle& style) { style.drag_handle = true; });
    }

    StyleBuilder& handle(float value) {
      return add(
        [value](UIStyle& style) { style.resize_handle_thickness = value; }
      );
    }

    StyleBuilder& corner(float value) {
      return add([value](UIStyle& style) { style.resize_corner_extent = value; });
    }

    StyleBuilder& accent_color(glm::vec4 color) {
      return add([color](UIStyle& style) { style.accent_color = color; });
    }

    StyleBuilder& control_gap(float value) {
      return add([value](UIStyle& style) { style.control_gap = value; });
    }

    StyleBuilder& control_indicator_size(float value) {
      return add(
        [value](UIStyle& style) { style.control_indicator_size = value; }
      );
    }

    StyleBuilder& slider_track_thickness(float value) {
      return add(
        [value](UIStyle& style) { style.slider_track_thickness = value; }
      );
    }

    StyleBuilder& slider_thumb_radius(float value) {
      return add([value](UIStyle& style) { style.slider_thumb_radius = value; });
    }

    StyleBuilder& hover(StateStyleRule rule) {
      return add([rule = std::move(rule)](UIStyle& style) {
        if (rule) {
          rule(style.hovered_style);
        }
        });
    }

    StyleBuilder& pressed(StateStyleRule rule) {
      return add([rule = std::move(rule)](UIStyle& style) {
        if (rule) {
          rule(style.pressed_style);
        }
        });
    }

    StyleBuilder& focused(StateStyleRule rule) {
      return add([rule = std::move(rule)](UIStyle& style) {
        if (rule) {
          rule(style.focused_style);
        }
        });
    }

    StyleBuilder& disabled(StateStyleRule rule) {
      return add([rule = std::move(rule)](UIStyle& style) {
        if (rule) {
          rule(style.disabled_style);
        }
        });
    }

    StyleBuilder& raw(StyleRule rule) { return add(std::move(rule)); }

    operator StyleRule() const {
      const auto rules = m_rules;
      return [rules](UIStyle& style) {
        for (const auto& rule : rules) {
          if (rule) {
            rule(style);
          }
        }
        };
    }

  private:
    StyleBuilder& add(StyleRule rule) {
      if (rule) {
        m_rules.push_back(std::move(rule));
      }

      return *this;
    }

    std::vector<StyleRule> m_rules;
  };

  namespace styles {

    inline UILength px(float value) { return UILength::pixels(value); }

    inline UILength percent(float value) { return UILength::percent(value); }

    inline UILength rem(float value) { return UILength::rem(value); }

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

    inline StyleBuilder row() { return StyleBuilder{}.row(); }
    inline StyleBuilder column() { return StyleBuilder{}.column(); }

    inline StyleBuilder items_start() { return StyleBuilder{}.items_start(); }
    inline StyleBuilder items_end() { return StyleBuilder{}.items_end(); }
    inline StyleBuilder items_center() { return StyleBuilder{}.items_center(); }

    inline StyleBuilder justify_start() { return StyleBuilder{}.justify_start(); }
    inline StyleBuilder justify_end() { return StyleBuilder{}.justify_end(); }
    inline StyleBuilder justify_center() { return StyleBuilder{}.justify_center(); }

    inline StyleBuilder gap(float value) { return StyleBuilder{}.gap(value); }

    inline StyleBuilder grow(float value = 1.0f) { return StyleBuilder{}.grow(value); }

    inline StyleBuilder shrink(float value = 1.0f) {
      return StyleBuilder{}.shrink(value);
    }

    inline StyleBuilder basis(UILength value) { return StyleBuilder{}.basis(value); }

    inline StyleBuilder flex(float value = 1.0f) { return StyleBuilder{}.flex(value); }

    inline StyleBuilder width(UILength value) { return StyleBuilder{}.width(value); }

    inline StyleBuilder height(UILength value) { return StyleBuilder{}.height(value); }

    inline StyleBuilder left(UILength value) { return StyleBuilder{}.left(value); }

    inline StyleBuilder top(UILength value) { return StyleBuilder{}.top(value); }

    inline StyleBuilder right(UILength value) { return StyleBuilder{}.right(value); }

    inline StyleBuilder bottom(UILength value) { return StyleBuilder{}.bottom(value); }

    inline StyleBuilder fill_x() { return StyleBuilder{}.fill_x(); }

    inline StyleBuilder fill_y() { return StyleBuilder{}.fill_y(); }

    inline StyleBuilder fill() { return StyleBuilder{}.fill(); }

    inline StyleBuilder padding(float value) { return StyleBuilder{}.padding(value); }

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

    inline StyleBuilder font_size(float value) {
      return StyleBuilder{}.font_size(value);
    }

    inline StyleBuilder overflow_hidden() { return StyleBuilder{}.overflow_hidden(); }

    inline StyleBuilder scroll_both() { return StyleBuilder{}.scroll_both(); }

    inline StyleBuilder scrollbar_auto() { return StyleBuilder{}.scrollbar_auto(); }

    inline StyleBuilder hover(StateStyleRule rule) {
      return StyleBuilder{}.hover(std::move(rule));
    }

    inline StyleBuilder pressed(StateStyleRule rule) {
      return StyleBuilder{}.pressed(std::move(rule));
    }

    inline StyleBuilder focused(StateStyleRule rule) {
      return StyleBuilder{}.focused(std::move(rule));
    }

    inline StyleBuilder disabled(StateStyleRule rule) {
      return StyleBuilder{}.disabled(std::move(rule));
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

    inline StyleBuilder raw(StyleRule rule) {
      return StyleBuilder{}.raw(std::move(rule));
    }

  } // namespace styles

  enum class NodeKind : uint8_t {
    View,
    Text,
    Image,
    Pressable,
    IconButton,
    SegmentedControl,
    ChipGroup,
    TextInput,
    ScrollView,
    Splitter,
    Button,
    Checkbox,
    Slider,
    Select,
  };

  struct NodeSpec {
    NodeKind kind = NodeKind::View;
    std::string name;
    std::string text;
    std::string placeholder;
    ResourceDescriptorID texture_id;
    std::vector<std::string> option_values;
    std::vector<bool> chip_selected_values;

    std::vector<StyleRule> style_rules;
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

    UINodeId* bound_id = nullptr;

    std::function<void()> on_hover_callback;
    std::function<void()> on_press_callback;
    std::function<void()> on_release_callback;
    std::function<void()> on_click_callback;
    std::function<void()> on_focus_callback;
    std::function<void()> on_blur_callback;
    std::function<void(const UIKeyInputEvent&)> on_key_input_callback;
    std::function<void(const UICharacterInputEvent&)>
      on_character_input_callback;
    std::function<void(const UIMouseWheelInputEvent&)> on_mouse_wheel_callback;
    std::function<void(const std::string&)> on_change_callback;
    std::function<void(const std::string&)> on_submit_callback;
    std::function<void(bool)> on_toggle_callback;
    std::function<void(float)> on_value_change_callback;
    std::function<void(size_t, const std::string&)> on_select_callback;
    std::function<void(size_t, const std::string&, bool)>
      on_chip_toggle_callback;

    NodeSpec& bind(UINodeId& target) {
      bound_id = &target;
      return *this;
    }

    NodeSpec& style() { return *this; }

    template <typename Rule, typename... Rest>
    NodeSpec& style(Rule&& rule, Rest &&...rest) {
      style_rules.emplace_back(StyleRule(std::forward<Rule>(rule)));
      if constexpr (sizeof...(rest) > 0) {
        style(std::forward<Rest>(rest)...);
      }

      return *this;
    }

    NodeSpec& children() { return *this; }

    template <typename Child, typename... Rest>
    NodeSpec& children(Child&& child, Rest &&...rest) {
      child_specs.emplace_back(std::forward<Child>(child));
      if constexpr (sizeof...(rest) > 0) {
        children(std::forward<Rest>(rest)...);
      }

      return *this;
    }

    NodeSpec& child(NodeSpec child) {
      child_specs.push_back(std::move(child));
      return *this;
    }

    NodeSpec& visible(bool value) {
      visible_value = value;
      return *this;
    }

    NodeSpec& enabled(bool value) {
      enabled_value = value;
      return *this;
    }

    NodeSpec& focusable(bool value) {
      focusable_value = value;
      return *this;
    }

    NodeSpec& read_only(bool value) {
      read_only_value = value;
      return *this;
    }

    NodeSpec& select_all_on_focus(bool value) {
      select_all_on_focus_value = value;
      return *this;
    }

    NodeSpec& checked(bool value) {
      checked_value = value;
      return *this;
    }

    NodeSpec& range(float min_value, float max_value) {
      slider_min_value = min_value;
      slider_max_value = max_value;
      return *this;
    }

    NodeSpec& step(float value) {
      slider_step_value = value;
      return *this;
    }

    NodeSpec& value(float next_value) {
      slider_value = next_value;
      return *this;
    }

    NodeSpec& options(std::vector<std::string> values) {
      option_values = std::move(values);
      return *this;
    }

    NodeSpec& selected(size_t index) {
      selected_index_value = index;
      return *this;
    }

    NodeSpec& selected_chips(std::vector<bool> values) {
      chip_selected_values = std::move(values);
      return *this;
    }

    NodeSpec& on_hover(std::function<void()> callback) {
      on_hover_callback = std::move(callback);
      return *this;
    }

    NodeSpec& on_press(std::function<void()> callback) {
      on_press_callback = std::move(callback);
      return *this;
    }

    NodeSpec& on_release(std::function<void()> callback) {
      on_release_callback = std::move(callback);
      return *this;
    }

    NodeSpec& on_click(std::function<void()> callback) {
      on_click_callback = std::move(callback);
      return *this;
    }

    NodeSpec& on_focus(std::function<void()> callback) {
      on_focus_callback = std::move(callback);
      return *this;
    }

    NodeSpec& on_blur(std::function<void()> callback) {
      on_blur_callback = std::move(callback);
      return *this;
    }

    NodeSpec&
      on_key_input(std::function<void(const UIKeyInputEvent&)> callback) {
      on_key_input_callback = std::move(callback);
      return *this;
    }

    NodeSpec& on_character_input(
      std::function<void(const UICharacterInputEvent&)> callback
    ) {
      on_character_input_callback = std::move(callback);
      return *this;
    }

    NodeSpec&
      on_mouse_wheel(std::function<void(const UIMouseWheelInputEvent&)> callback) {
      on_mouse_wheel_callback = std::move(callback);
      return *this;
    }

    NodeSpec& on_change(std::function<void(const std::string&)> callback) {
      on_change_callback = std::move(callback);
      return *this;
    }

    NodeSpec& on_submit(std::function<void(const std::string&)> callback) {
      on_submit_callback = std::move(callback);
      return *this;
    }

    NodeSpec& on_toggle(std::function<void(bool)> callback) {
      on_toggle_callback = std::move(callback);
      return *this;
    }

    NodeSpec& on_value_change(std::function<void(float)> callback) {
      on_value_change_callback = std::move(callback);
      return *this;
    }

    NodeSpec&
      on_select(std::function<void(size_t, const std::string&)> callback) {
      on_select_callback = std::move(callback);
      return *this;
    }

    NodeSpec& on_chip_toggle(
      std::function<void(size_t, const std::string&, bool)> callback
    ) {
      on_chip_toggle_callback = std::move(callback);
      return *this;
    }
  };

  inline NodeSpec view(std::string name = {}) {
    return NodeSpec{ .kind = NodeKind::View, .name = std::move(name) };
  }

  inline NodeSpec row(std::string name = {}) {
    auto spec = view(std::move(name));
    spec.style(styles::row());
    return spec;
  }

  inline NodeSpec column(std::string name = {}) {
    auto spec = view(std::move(name));
    spec.style(styles::column());
    return spec;
  }

  inline NodeSpec text(std::string value = {}, std::string name = {}) {
    return NodeSpec{
        .kind = NodeKind::Text,
        .name = std::move(name),
        .text = std::move(value),
    };
  }

  inline NodeSpec image(ResourceDescriptorID texture_id = {}, std::string name = {}) {
    return NodeSpec{
        .kind = NodeKind::Image,
        .name = std::move(name),
        .texture_id = std::move(texture_id),
    };
  }

  inline NodeSpec pressable(std::string name = {}) {
    return NodeSpec{ .kind = NodeKind::Pressable, .name = std::move(name) };
  }

  inline NodeSpec icon_button(ResourceDescriptorID texture_id = {}, std::function<void()> on_click = {}, std::string name = {}) {
    return NodeSpec{
        .kind = NodeKind::IconButton,
        .name = std::move(name),
        .texture_id = std::move(texture_id),
        .on_click_callback = std::move(on_click),
    };
  }

  inline NodeSpec text_input(std::string value = {}, std::string placeholder = {}, std::string name = {}) {
    return NodeSpec{
        .kind = NodeKind::TextInput,
        .name = std::move(name),
        .text = std::move(value),
        .placeholder = std::move(placeholder),
    };
  }

  inline NodeSpec input(std::string value = {}, std::string placeholder = {}, std::string name = {}) {
    return text_input(std::move(value), std::move(placeholder), std::move(name));
  }

  inline NodeSpec scroll_view(std::string name = {}) {
    return NodeSpec{ .kind = NodeKind::ScrollView, .name = std::move(name) };
  }

  inline NodeSpec splitter(std::string name = {}) {
    return NodeSpec{ .kind = NodeKind::Splitter, .name = std::move(name) };
  }

  inline NodeSpec button(std::string label, std::function<void()> on_click = {}, std::string name = {}) {
    return NodeSpec{
        .kind = NodeKind::Button,
        .name = std::move(name),
        .text = std::move(label),
        .on_click_callback = std::move(on_click),
    };
  }

  inline NodeSpec checkbox(std::string label = {}, bool checked = false, std::string name = {}) {
    return NodeSpec{
        .kind = NodeKind::Checkbox,
        .name = std::move(name),
        .text = std::move(label),
        .checked_value = checked,
    };
  }

  inline NodeSpec slider(float value = 0.0f, float min_value = 0.0f, float max_value = 1.0f, std::string name = {}) {
    return NodeSpec{
        .kind = NodeKind::Slider,
        .name = std::move(name),
        .slider_value = value,
        .slider_min_value = min_value,
        .slider_max_value = max_value,
    };
  }

  inline NodeSpec select(std::vector<std::string> options = {}, size_t selected_index = 0u, std::string placeholder = {}, std::string name = {}) {
    return NodeSpec{
        .kind = NodeKind::Select,
        .name = std::move(name),
        .placeholder = std::move(placeholder),
        .option_values = std::move(options),
        .selected_index_value = selected_index,
    };
  }

  inline NodeSpec segmented_control(std::vector<std::string> options = {}, size_t selected_index = 0u, std::string name = {}) {
    return NodeSpec{
        .kind = NodeKind::SegmentedControl,
        .name = std::move(name),
        .option_values = std::move(options),
        .selected_index_value = selected_index,
    };
  }

  inline NodeSpec chip_group(std::vector<std::string> options = {}, std::vector<bool> selected = {}, std::string name = {}) {
    return NodeSpec{
        .kind = NodeKind::ChipGroup,
        .name = std::move(name),
        .option_values = std::move(options),
        .chip_selected_values = std::move(selected),
    };
  }

  inline NodeSpec spacer(std::string name = {}) {
    auto spec = view(std::move(name));
    spec.style(styles::grow(1.0f));
    return spec;
  }

  namespace detail {

    inline bool spec_allows_children(NodeKind kind) {
      switch (kind) {
      case NodeKind::View:
      case NodeKind::Pressable:
      case NodeKind::ScrollView:
        return true;
      case NodeKind::Text:
      case NodeKind::Image:
      case NodeKind::IconButton:
      case NodeKind::SegmentedControl:
      case NodeKind::ChipGroup:
      case NodeKind::TextInput:
      case NodeKind::Splitter:
      case NodeKind::Button:
      case NodeKind::Checkbox:
      case NodeKind::Slider:
      case NodeKind::Select:
      default:
        return false;
      }
    }

    inline void validate_spec(const NodeSpec& spec) {
      ASTRA_ENSURE(!spec_allows_children(spec.kind) && !spec.child_specs.empty(), "ui::dsl leaf nodes cannot declare children");
    }

    inline UINodeId create_node(UIDocument& document, const NodeSpec& spec) {
      switch (spec.kind) {
      case NodeKind::View:
        return document.create_view(spec.name);
      case NodeKind::Text:
        return document.create_text(spec.text, spec.name);
      case NodeKind::Image:
        return document.create_image(spec.texture_id, spec.name);
      case NodeKind::Pressable:
        return document.create_pressable(spec.name);
      case NodeKind::IconButton:
        return document.create_icon_button(spec.texture_id, spec.on_click_callback, spec.name);
      case NodeKind::SegmentedControl:
        return document.create_segmented_control(
          spec.option_values, spec.selected_index_value.value_or(0u), spec.name
        );
      case NodeKind::ChipGroup:
        return document.create_chip_group(spec.option_values, spec.chip_selected_values, spec.name);
      case NodeKind::TextInput:
        return document.create_text_input(spec.text, spec.placeholder, spec.name);
      case NodeKind::ScrollView:
        return document.create_scroll_view(spec.name);
      case NodeKind::Splitter:
        return document.create_splitter(spec.name);
      case NodeKind::Button:
        return document.create_button(spec.text, spec.on_click_callback, spec.name);
      case NodeKind::Checkbox:
        return document.create_checkbox(
          spec.text, spec.checked_value.value_or(false), spec.name
        );
      case NodeKind::Slider:
        return document.create_slider(
          spec.slider_value.value_or(0.0f), spec.slider_min_value.value_or(0.0f), spec.slider_max_value.value_or(1.0f), spec.slider_step_value.value_or(0.1f), spec.name
        );
      case NodeKind::Select:
        return document.create_select(spec.option_values, spec.selected_index_value.value_or(0u), spec.placeholder, spec.name);
      }

      return k_invalid_node_id;
    }

    inline void apply_properties(UIDocument& document, UINodeId node_id, const NodeSpec& spec) {
      if (spec.bound_id != nullptr) {
        *spec.bound_id = node_id;
      }

      if (spec.visible_value.has_value()) {
        document.set_visible(node_id, *spec.visible_value);
      }

      if (spec.enabled_value.has_value()) {
        document.set_enabled(node_id, *spec.enabled_value);
      }

      if (spec.focusable_value.has_value()) {
        document.set_focusable(node_id, *spec.focusable_value);
      }

      if (spec.read_only_value.has_value()) {
        document.set_read_only(node_id, *spec.read_only_value);
      }

      if (spec.select_all_on_focus_value.has_value()) {
        document.set_select_all_on_focus(node_id, *spec.select_all_on_focus_value);
      }

      if (spec.checked_value.has_value()) {
        document.set_checked(node_id, *spec.checked_value);
      }

      if (spec.slider_min_value.has_value() || spec.slider_max_value.has_value() ||
        spec.slider_step_value.has_value()) {
        document.set_slider_range(node_id, spec.slider_min_value.value_or(0.0f), spec.slider_max_value.value_or(1.0f), spec.slider_step_value.value_or(0.1f));
      }

      if (spec.slider_value.has_value()) {
        document.set_slider_value(node_id, *spec.slider_value);
      }

      if (!spec.option_values.empty()) {
        document.set_select_options(node_id, spec.option_values);
        document.set_segmented_options(node_id, spec.option_values);
        document.set_chip_options(node_id, spec.option_values, spec.chip_selected_values);
      }

      if (spec.selected_index_value.has_value()) {
        document.set_selected_index(node_id, *spec.selected_index_value);
        document.set_segmented_selected_index(node_id, *spec.selected_index_value);
      }

      if (!spec.chip_selected_values.empty()) {
        document.set_chip_options(node_id, spec.option_values, spec.chip_selected_values);
      }

      if (spec.on_hover_callback) {
        document.set_on_hover(node_id, spec.on_hover_callback);
      }

      if (spec.on_press_callback) {
        document.set_on_press(node_id, spec.on_press_callback);
      }

      if (spec.on_release_callback) {
        document.set_on_release(node_id, spec.on_release_callback);
      }

      if (spec.on_click_callback && spec.kind != NodeKind::Button) {
        document.set_on_click(node_id, spec.on_click_callback);
      }

      if (spec.on_focus_callback) {
        document.set_on_focus(node_id, spec.on_focus_callback);
      }

      if (spec.on_blur_callback) {
        document.set_on_blur(node_id, spec.on_blur_callback);
      }

      if (spec.on_key_input_callback) {
        document.set_on_key_input(node_id, spec.on_key_input_callback);
      }

      if (spec.on_character_input_callback) {
        document.set_on_character_input(node_id, spec.on_character_input_callback);
      }

      if (spec.on_mouse_wheel_callback) {
        document.set_on_mouse_wheel(node_id, spec.on_mouse_wheel_callback);
      }

      if (spec.on_change_callback) {
        document.set_on_change(node_id, spec.on_change_callback);
      }

      if (spec.on_submit_callback) {
        document.set_on_submit(node_id, spec.on_submit_callback);
      }

      if (spec.on_toggle_callback) {
        document.set_on_toggle(node_id, spec.on_toggle_callback);
      }

      if (spec.on_value_change_callback) {
        document.set_on_value_change(node_id, spec.on_value_change_callback);
      }

      if (spec.on_select_callback) {
        document.set_on_select(node_id, spec.on_select_callback);
      }

      if (spec.on_chip_toggle_callback) {
        document.set_on_chip_toggle(node_id, spec.on_chip_toggle_callback);
      }

      for (const auto& rule : spec.style_rules) {
        if (rule) {
          document.mutate_style(node_id, rule);
        }
      }
    }

    inline UINodeId materialize(UIDocument& document, std::optional<UINodeId> parent_id, const NodeSpec& spec) {
      validate_spec(spec);

      const UINodeId node_id = create_node(document, spec);
      if (parent_id.has_value()) {
        document.append_child(*parent_id, node_id);
      }

      apply_properties(document, node_id, spec);
      for (const auto& child : spec.child_specs) {
        materialize(document, node_id, child);
      }

      return node_id;
    }

  } // namespace detail

  inline UINodeId mount(UIDocument& document, const NodeSpec& spec) {
    const UINodeId root_id = detail::materialize(document, std::nullopt, spec);
    document.set_root(root_id);
    return root_id;
  }

  inline UINodeId append(UIDocument& document, UINodeId parent_id, const NodeSpec& spec) {
    ASTRA_ENSURE(parent_id == k_invalid_node_id, "ui::dsl append requires a valid parent");
    return detail::materialize(document, parent_id, spec);
  }

} // namespace astralix::ui::dsl
