#include "document.hpp"

#include "assert.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace astralix::ui {

namespace {

size_t clamp_text_index(const UIDocument::UINode &node, size_t index) {
  return std::min(index, node.text.size());
}

UITextSelection clamp_text_selection(const UIDocument::UINode &node, UITextSelection selection) {
  selection.anchor = clamp_text_index(node, selection.anchor);
  selection.focus = clamp_text_index(node, selection.focus);
  return selection;
}

void clamp_text_runtime_state(UIDocument::UINode &node) {
  node.selection = clamp_text_selection(node, node.selection);
  node.caret.index = clamp_text_index(node, node.caret.index);
  node.text_scroll_x = std::max(0.0f, node.text_scroll_x);
}

float sanitized_slider_step(float min_value, float max_value, float step) {
  if (step > 0.0f) {
    return step;
  }

  const float span = std::abs(max_value - min_value);
  return span > 0.0f ? span / 100.0f : 1.0f;
}

float clamp_slider_value(const UISliderState &slider, float value) {
  const float clamped =
      std::clamp(value, slider.min_value, slider.max_value);
  if (slider.step <= 0.0f) {
    return clamped;
  }

  const float steps = std::round((clamped - slider.min_value) / slider.step);
  return std::clamp(slider.min_value + steps * slider.step, slider.min_value, slider.max_value);
}

void normalize_slider_state(UISliderState &slider) {
  if (slider.max_value < slider.min_value) {
    std::swap(slider.min_value, slider.max_value);
  }

  slider.step =
      sanitized_slider_step(slider.min_value, slider.max_value, slider.step);
  slider.value = clamp_slider_value(slider, slider.value);
}

void normalize_select_state(UISelectState &select) {
  if (select.options.empty()) {
    select.selected_index = 0u;
    select.highlighted_index = 0u;
    select.open = false;
    return;
  }

  const size_t max_index = select.options.size() - 1u;
  select.selected_index = std::min(select.selected_index, max_index);
  select.highlighted_index = std::min(select.highlighted_index, max_index);
}

void normalize_segmented_control_state(UISegmentedControlState &segmented) {
  if (segmented.options.empty()) {
    segmented.selected_index = 0u;
    return;
  }

  segmented.selected_index =
      std::min(segmented.selected_index, segmented.options.size() - 1u);
}

void normalize_chip_group_state(UIChipGroupState &chip_group) {
  if (chip_group.options.empty()) {
    chip_group.selected.clear();
    return;
  }

  if (chip_group.selected.size() != chip_group.options.size()) {
    const size_t previous_size = chip_group.selected.size();
    chip_group.selected.resize(chip_group.options.size(), true);
    for (size_t index = previous_size; index < chip_group.selected.size();
         ++index) {
      chip_group.selected[index] = true;
    }
  }
}

} // namespace

Ref<UIDocument> UIDocument::create() { return create_ref<UIDocument>(); }

UINodeId UIDocument::allocate_node(NodeType type, std::string name) {
  if (m_nodes.empty()) {
    m_nodes.push_back(NodeSlot{});
  }

  UINodeId node_id = static_cast<UINodeId>(m_nodes.size());
  m_nodes.push_back(NodeSlot{
      .node = UINode{
          .id = node_id,
          .type = type,
          .name = std::move(name),
      },
      .alive = true,
  });

  m_structure_dirty = true;
  m_layout_dirty = true;
  m_paint_dirty = true;
  return node_id;
}

bool UIDocument::is_valid_node(UINodeId node_id) const {
  return node_id != k_invalid_node_id && node_id < m_nodes.size() &&
         m_nodes[node_id].alive;
}

UIDocument::UINode *UIDocument::node(UINodeId node_id) {
  return is_valid_node(node_id) ? &m_nodes[node_id].node : nullptr;
}

const UIDocument::UINode *UIDocument::node(UINodeId node_id) const {
  return is_valid_node(node_id) ? &m_nodes[node_id].node : nullptr;
}

UINodeId UIDocument::create_view(std::string name) {
  return allocate_node(NodeType::View, std::move(name));
}

UINodeId UIDocument::create_text(std::string text, std::string name) {
  UINodeId node_id = allocate_node(NodeType::Text, std::move(name));
  set_text(node_id, std::move(text));
  return node_id;
}

UINodeId UIDocument::create_image(ResourceDescriptorID texture_id, std::string name) {
  UINodeId node_id = allocate_node(NodeType::Image, std::move(name));
  set_texture(node_id, std::move(texture_id));
  return node_id;
}

UINodeId UIDocument::create_pressable(std::string name) {
  UINodeId node_id = allocate_node(NodeType::Pressable, std::move(name));
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->focusable = true;
  }

  return node_id;
}

UINodeId UIDocument::create_icon_button(ResourceDescriptorID texture_id, const std::function<void()> &on_click, std::string name) {
  UINodeId button_id = create_pressable(std::move(name));
  UINodeId icon_id = create_image(std::move(texture_id), "icon");
  append_child(button_id, icon_id);
  set_on_click(button_id, on_click);

  mutate_style(button_id, [](UIStyle &style) {
    style.padding = UIEdges::all(8.0f);
    style.width = UILength::pixels(34.0f);
    style.height = UILength::pixels(34.0f);
    style.border_radius = 10.0f;
    style.background_color = glm::vec4(0.08f, 0.13f, 0.2f, 0.88f);
    style.border_color = glm::vec4(0.34f, 0.47f, 0.63f, 0.35f);
    style.border_width = 1.0f;
    style.align_items = AlignItems::Center;
    style.justify_content = JustifyContent::Center;
    style.flex_shrink = 0.0f;
    style.hovered_style.background_color =
        glm::vec4(0.12f, 0.19f, 0.29f, 0.94f);
    style.pressed_style.background_color =
        glm::vec4(0.07f, 0.12f, 0.19f, 0.98f);
    style.focused_style.border_color = glm::vec4(0.8f, 0.88f, 1.0f, 0.95f);
    style.focused_style.border_width = 2.0f;
    style.disabled_style.opacity = 0.55f;
  });

  mutate_style(icon_id, [](UIStyle &style) {
    style.width = UILength::pixels(16.0f);
    style.height = UILength::pixels(16.0f);
    style.tint = glm::vec4(0.9f, 0.96f, 1.0f, 1.0f);
  });

  return button_id;
}

UINodeId UIDocument::create_segmented_control(std::vector<std::string> options, size_t selected_index, std::string name) {
  UINodeId node_id =
      allocate_node(NodeType::SegmentedControl, std::move(name));
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->focusable = true;
    node->segmented_control.options = std::move(options);
    node->segmented_control.selected_index = selected_index;
    normalize_segmented_control_state(node->segmented_control);
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.padding = UIEdges::all(4.0f);
    style.border_radius = 999.0f;
    style.border_width = 1.0f;
    style.background_color = glm::vec4(0.04f, 0.08f, 0.13f, 0.9f);
    style.border_color = glm::vec4(0.36f, 0.48f, 0.61f, 0.45f);
    style.text_color = glm::vec4(0.94f, 0.97f, 1.0f, 1.0f);
    style.control_gap = 4.0f;
    style.focused_style.border_color = glm::vec4(0.74f, 0.84f, 0.98f, 0.95f);
    style.focused_style.border_width = 2.0f;
    style.disabled_style.opacity = 0.6f;
  });

  return node_id;
}

UINodeId UIDocument::create_chip_group(std::vector<std::string> options, std::vector<bool> selected, std::string name) {
  UINodeId node_id = allocate_node(NodeType::ChipGroup, std::move(name));
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->chip_group.options = std::move(options);
    node->chip_group.selected = std::move(selected);
    normalize_chip_group_state(node->chip_group);
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.padding = UIEdges::all(0.0f);
    style.border_radius = 999.0f;
    style.text_color = glm::vec4(0.94f, 0.97f, 1.0f, 1.0f);
    style.control_gap = 8.0f;
    style.disabled_style.opacity = 0.6f;
  });

  return node_id;
}

UINodeId UIDocument::create_text_input(std::string value, std::string placeholder, std::string name) {
  UINodeId node_id = allocate_node(NodeType::TextInput, std::move(name));
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->focusable = true;
    node->text = std::move(value);
    node->placeholder = std::move(placeholder);
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.padding = UIEdges::symmetric(12.0f, 10.0f);
    style.border_radius = 10.0f;
    style.border_width = 1.0f;
    style.overflow = Overflow::Hidden;
    style.background_color = glm::vec4(0.04f, 0.08f, 0.13f, 0.9f);
    style.border_color = glm::vec4(0.36f, 0.48f, 0.61f, 0.45f);
    style.text_color = glm::vec4(0.94f, 0.97f, 1.0f, 1.0f);
    style.placeholder_text_color = glm::vec4(0.59f, 0.68f, 0.78f, 0.88f);
    style.disabled_style.opacity = 0.6f;
  });

  return node_id;
}

UINodeId UIDocument::create_scroll_view(std::string name) {
  UINodeId node_id = allocate_node(NodeType::ScrollView, std::move(name));
  mutate_style(node_id, [](UIStyle &style) {
    style.overflow = Overflow::Hidden;
    style.scroll_mode = ScrollMode::Vertical;
    style.scrollbar_visibility = ScrollbarVisibility::Auto;
  });
  return node_id;
}

UINodeId UIDocument::create_splitter(std::string name) {
  UINodeId node_id = allocate_node(NodeType::Splitter, std::move(name));
  mutate_style(node_id, [](UIStyle &style) {
    style.align_self = AlignSelf::Stretch;
    style.flex_shrink = 0.0f;
    style.background_color = glm::vec4(0.2f, 0.31f, 0.44f, 0.38f);
    style.hovered_style.background_color =
        glm::vec4(0.42f, 0.62f, 0.84f, 0.72f);
    style.pressed_style.background_color = glm::vec4(0.66f, 0.82f, 0.98f, 0.9f);
  });
  return node_id;
}

UINodeId UIDocument::create_checkbox(std::string label, bool checked, std::string name) {
  UINodeId node_id = allocate_node(NodeType::Checkbox, std::move(name));
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->focusable = true;
    node->text = std::move(label);
    node->checkbox.checked = checked;
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.padding = UIEdges::symmetric(10.0f, 8.0f);
    style.border_radius = 10.0f;
    style.text_color = glm::vec4(0.92f, 0.96f, 1.0f, 1.0f);
    style.hovered_style.background_color = glm::vec4(0.08f, 0.14f, 0.22f, 0.55f);
    style.pressed_style.background_color = glm::vec4(0.1f, 0.18f, 0.28f, 0.75f);
    style.focused_style.border_color = glm::vec4(0.78f, 0.88f, 1.0f, 0.92f);
    style.focused_style.border_width = 1.0f;
    style.disabled_style.opacity = 0.55f;
  });

  return node_id;
}

UINodeId UIDocument::create_slider(float value, float min_value, float max_value, float step, std::string name) {
  UINodeId node_id = allocate_node(NodeType::Slider, std::move(name));
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->focusable = true;
    node->slider = UISliderState{
        .value = value,
        .min_value = min_value,
        .max_value = max_value,
        .step = step,
    };
    normalize_slider_state(node->slider);
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.padding = UIEdges::symmetric(12.0f, 10.0f);
    style.border_radius = 12.0f;
    style.border_width = 1.0f;
    style.background_color = glm::vec4(0.04f, 0.08f, 0.13f, 0.8f);
    style.border_color = glm::vec4(0.34f, 0.46f, 0.6f, 0.42f);
    style.hovered_style.border_color = glm::vec4(0.5f, 0.64f, 0.82f, 0.72f);
    style.pressed_style.border_color = glm::vec4(0.68f, 0.82f, 1.0f, 0.92f);
    style.focused_style.border_color = glm::vec4(0.8f, 0.9f, 1.0f, 0.95f);
    style.focused_style.border_width = 2.0f;
    style.disabled_style.opacity = 0.55f;
  });

  return node_id;
}

UINodeId UIDocument::create_select(std::vector<std::string> options, size_t selected_index, std::string placeholder, std::string name) {
  UINodeId node_id = allocate_node(NodeType::Select, std::move(name));
  if (UINode *node = this->node(node_id); node != nullptr) {
    node->focusable = true;
    node->placeholder = std::move(placeholder);
    node->select.options = std::move(options);
    node->select.selected_index = selected_index;
    node->select.highlighted_index = selected_index;
    normalize_select_state(node->select);
  }

  mutate_style(node_id, [](UIStyle &style) {
    style.padding = UIEdges::symmetric(12.0f, 10.0f);
    style.border_radius = 10.0f;
    style.border_width = 1.0f;
    style.background_color = glm::vec4(0.04f, 0.08f, 0.13f, 0.9f);
    style.border_color = glm::vec4(0.36f, 0.48f, 0.61f, 0.45f);
    style.text_color = glm::vec4(0.94f, 0.97f, 1.0f, 1.0f);
    style.placeholder_text_color = glm::vec4(0.59f, 0.68f, 0.78f, 0.88f);
    style.hovered_style.border_color = glm::vec4(0.5f, 0.63f, 0.8f, 0.65f);
    style.focused_style.border_color = glm::vec4(0.74f, 0.84f, 0.98f, 0.95f);
    style.focused_style.border_width = 2.0f;
    style.disabled_style.opacity = 0.6f;
  });

  return node_id;
}

UINodeId UIDocument::create_button(const std::string &label, const std::function<void()> &on_click, std::string name) {
  UINodeId button_id = create_pressable(std::move(name));
  UINodeId label_id = create_text(label, "label");
  append_child(button_id, label_id);
  set_on_click(button_id, on_click);

  mutate_style(button_id, [](UIStyle &style) {
    style.padding = UIEdges::symmetric(14.0f, 10.0f);
    style.border_radius = 0.0f;
    style.background_color = glm::vec4(0.16f, 0.24f, 0.37f, 0.92f);
    style.border_color = glm::vec4(0.62f, 0.76f, 0.94f, 0.35f);
    style.border_width = 1.0f;
    style.align_items = AlignItems::Center;
    style.justify_content = JustifyContent::Center;
    style.hovered_style.background_color = glm::vec4(0.2f, 0.31f, 0.46f, 0.96f);
    style.pressed_style.background_color =
        glm::vec4(0.11f, 0.18f, 0.29f, 0.96f);
    style.focused_style.border_color = glm::vec4(0.8f, 0.88f, 1.0f, 0.95f);
    style.focused_style.border_width = 2.0f;
    style.disabled_style.opacity = 0.55f;
  });

  mutate_style(label_id, [](UIStyle &style) {
    style.text_color = glm::vec4(0.93f, 0.97f, 1.0f, 1.0f);
    style.font_size = 16.0f;
  });

  return button_id;
}

void UIDocument::set_root(UINodeId root_id) {
  ASTRA_ENSURE(!is_valid_node(root_id), "ui root node is invalid");
  m_root_id = root_id;
  m_structure_dirty = true;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::detach_from_parent(UINodeId child_id) {
  UINode *child = node(child_id);
  if (child == nullptr || child->parent == k_invalid_node_id) {
    return;
  }

  UINode *parent = node(child->parent);
  if (parent != nullptr) {
    auto &children = parent->children;
    children.erase(std::remove(children.begin(), children.end(), child_id), children.end());
  }

  child->parent = k_invalid_node_id;
}

void UIDocument::append_child(UINodeId parent_id, UINodeId child_id) {
  UINode *parent = node(parent_id);
  UINode *child = node(child_id);

  ASTRA_ENSURE(parent == nullptr || child == nullptr, "cannot append invalid ui child");
  ASTRA_ENSURE(parent_id == child_id, "ui node cannot be its own parent");

  detach_from_parent(child_id);
  parent->children.push_back(child_id);
  child->parent = parent_id;

  m_structure_dirty = true;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::remove_child(UINodeId child_id) {
  if (!is_valid_node(child_id)) {
    return;
  }

  detach_from_parent(child_id);
  m_structure_dirty = true;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::clear_children(UINodeId parent_id) {
  UINode *parent = node(parent_id);
  if (parent == nullptr) {
    return;
  }

  for (UINodeId child_id : parent->children) {
    if (UINode *child = node(child_id); child != nullptr) {
      child->parent = k_invalid_node_id;
    }
  }

  parent->children.clear();
  m_structure_dirty = true;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_style(UINodeId node_id, const UIStyle &style) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  target->style = style;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::mutate_style(UINodeId node_id, const std::function<void(UIStyle &)> &fn) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  fn(target->style);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_text(UINodeId node_id, std::string text) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (target->text == text) {
    return;
  }

  target->text = std::move(text);
  clamp_text_runtime_state(*target);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_texture(UINodeId node_id, ResourceDescriptorID texture_id) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (target->texture_id == texture_id) {
    return;
  }

  target->texture_id = std::move(texture_id);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_placeholder(UINodeId node_id, std::string placeholder) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (target->placeholder == placeholder) {
    return;
  }

  target->placeholder = std::move(placeholder);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_visible(UINodeId node_id, bool visible) {
  UINode *target = node(node_id);
  if (target == nullptr || target->visible == visible) {
    return;
  }

  if (!visible && target->type == NodeType::Select) {
    set_select_open(node_id, false);
  }

  target->visible = visible;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_enabled(UINodeId node_id, bool enabled) {
  UINode *target = node(node_id);
  if (target == nullptr || target->enabled == enabled) {
    return;
  }

  target->enabled = enabled;
  if (!enabled && target->type == NodeType::Select) {
    set_select_open(node_id, false);
  }
  if (!enabled) {
    if (m_hot_node == node_id) {
      set_hot_node(k_invalid_node_id);
    }

    if (m_active_node == node_id) {
      set_active_node(k_invalid_node_id);
    }

    if (m_focused_node == node_id) {
      set_focused_node(k_invalid_node_id);
    }
  }

  m_paint_dirty = true;
}

void UIDocument::set_focusable(UINodeId node_id, bool focusable) {
  UINode *target = node(node_id);
  if (target == nullptr || target->focusable == focusable) {
    return;
  }

  target->focusable = focusable;
  if (!focusable && target->type == NodeType::Select) {
    set_select_open(node_id, false);
  }
  if (!focusable && m_focused_node == node_id) {
    set_focused_node(k_invalid_node_id);
  }
}

void UIDocument::set_read_only(UINodeId node_id, bool read_only) {
  UINode *target = node(node_id);
  if (target == nullptr || target->read_only == read_only) {
    return;
  }

  target->read_only = read_only;
  m_paint_dirty = true;
}

void UIDocument::set_select_all_on_focus(UINodeId node_id, bool select_all_on_focus) {
  UINode *target = node(node_id);
  if (target == nullptr || target->select_all_on_focus == select_all_on_focus) {
    return;
  }

  target->select_all_on_focus = select_all_on_focus;
}

void UIDocument::set_checked(UINodeId node_id, bool checked) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Checkbox ||
      target->checkbox.checked == checked) {
    return;
  }

  target->checkbox.checked = checked;
  m_paint_dirty = true;
}

bool UIDocument::checked(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Checkbox) {
    return false;
  }

  return target->checkbox.checked;
}

void UIDocument::set_slider_value(UINodeId node_id, float value) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Slider) {
    return;
  }

  const float clamped = clamp_slider_value(target->slider, value);
  if (target->slider.value == clamped) {
    return;
  }

  target->slider.value = clamped;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

float UIDocument::slider_value(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Slider) {
    return 0.0f;
  }

  return target->slider.value;
}

void UIDocument::set_slider_range(UINodeId node_id, float min_value, float max_value, float step) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Slider) {
    return;
  }

  const UISliderState previous = target->slider;
  target->slider.min_value = min_value;
  target->slider.max_value = max_value;
  target->slider.step = step;
  normalize_slider_state(target->slider);

  if (target->slider.min_value == previous.min_value &&
      target->slider.max_value == previous.max_value &&
      target->slider.step == previous.step &&
      target->slider.value == previous.value) {
    return;
  }

  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_select_options(UINodeId node_id, std::vector<std::string> options) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Select) {
    return;
  }

  target->select.options = std::move(options);
  normalize_select_state(target->select);
  if (target->select.options.empty()) {
    set_select_open(node_id, false);
  } else {
    m_layout_dirty = true;
    m_paint_dirty = true;
  }
}

const std::vector<std::string> *
UIDocument::select_options(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Select) {
    return nullptr;
  }

  return &target->select.options;
}

void UIDocument::set_selected_index(UINodeId node_id, size_t selected_index) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Select) {
    return;
  }

  const size_t previous_selected = target->select.selected_index;
  target->select.selected_index = selected_index;
  target->select.highlighted_index = selected_index;
  normalize_select_state(target->select);

  if (target->select.selected_index == previous_selected) {
    return;
  }

  m_layout_dirty = true;
  m_paint_dirty = true;
}

size_t UIDocument::selected_index(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Select) {
    return 0u;
  }

  return target->select.selected_index;
}

void UIDocument::set_select_open(UINodeId node_id, bool open) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Select) {
    return;
  }

  if (open && (!target->visible || !target->enabled ||
               target->select.options.empty())) {
    open = false;
  }

  if (open && m_open_select_node != k_invalid_node_id &&
      m_open_select_node != node_id) {
    if (UINode *previous = node(m_open_select_node); previous != nullptr) {
      previous->select.open = false;
    }
    m_open_select_node = k_invalid_node_id;
    m_layout_dirty = true;
    m_paint_dirty = true;
  }

  if (target->select.open == open &&
      (!open || m_open_select_node == node_id)) {
    return;
  }

  target->select.open = open;
  if (open) {
    target->select.highlighted_index = target->select.selected_index;
    normalize_select_state(target->select);
    m_open_select_node = node_id;
  } else if (m_open_select_node == node_id) {
    m_open_select_node = k_invalid_node_id;
  }

  m_layout_dirty = true;
  m_paint_dirty = true;
}

bool UIDocument::select_open(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::Select) {
    return false;
  }

  return target->select.open;
}

void UIDocument::set_segmented_options(UINodeId node_id, std::vector<std::string> options) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::SegmentedControl) {
    return;
  }

  target->segmented_control.options = std::move(options);
  normalize_segmented_control_state(target->segmented_control);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

const std::vector<std::string> *
UIDocument::segmented_options(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::SegmentedControl) {
    return nullptr;
  }

  return &target->segmented_control.options;
}

void UIDocument::set_segmented_selected_index(UINodeId node_id, size_t selected_index) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::SegmentedControl) {
    return;
  }

  const size_t previous_selected = target->segmented_control.selected_index;
  target->segmented_control.selected_index = selected_index;
  normalize_segmented_control_state(target->segmented_control);

  if (target->segmented_control.selected_index == previous_selected) {
    return;
  }

  m_paint_dirty = true;
}

size_t UIDocument::segmented_selected_index(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::SegmentedControl) {
    return 0u;
  }

  return target->segmented_control.selected_index;
}

void UIDocument::set_chip_options(UINodeId node_id, std::vector<std::string> options, std::vector<bool> selected) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::ChipGroup) {
    return;
  }

  target->chip_group.options = std::move(options);
  target->chip_group.selected = std::move(selected);
  normalize_chip_group_state(target->chip_group);
  m_layout_dirty = true;
  m_paint_dirty = true;
}

const std::vector<std::string> *UIDocument::chip_options(UINodeId node_id) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::ChipGroup) {
    return nullptr;
  }

  return &target->chip_group.options;
}

void UIDocument::set_chip_selected(UINodeId node_id, size_t index, bool selected) {
  UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::ChipGroup) {
    return;
  }

  normalize_chip_group_state(target->chip_group);
  if (index >= target->chip_group.selected.size() ||
      target->chip_group.selected[index] == selected) {
    return;
  }

  target->chip_group.selected[index] = selected;
  m_paint_dirty = true;
}

bool UIDocument::chip_selected(UINodeId node_id, size_t index) const {
  const UINode *target = node(node_id);
  if (target == nullptr || target->type != NodeType::ChipGroup) {
    return false;
  }

  if (index >= target->chip_group.selected.size()) {
    return false;
  }

  return target->chip_group.selected[index];
}

void UIDocument::set_on_hover(UINodeId node_id, std::function<void()> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_hover = std::move(callback);
  }
}

void UIDocument::set_on_press(UINodeId node_id, std::function<void()> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_press = std::move(callback);
  }
}

void UIDocument::set_on_release(UINodeId node_id, std::function<void()> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_release = std::move(callback);
  }
}

void UIDocument::set_on_click(UINodeId node_id, std::function<void()> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_click = std::move(callback);
  }
}

void UIDocument::set_on_focus(UINodeId node_id, std::function<void()> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_focus = std::move(callback);
  }
}

void UIDocument::set_on_blur(UINodeId node_id, std::function<void()> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_blur = std::move(callback);
  }
}

void UIDocument::set_on_key_input(
    UINodeId node_id, std::function<void(const UIKeyInputEvent &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_key_input = std::move(callback);
  }
}

void UIDocument::set_on_character_input(
    UINodeId node_id,
    std::function<void(const UICharacterInputEvent &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_character_input = std::move(callback);
  }
}

void UIDocument::set_on_mouse_wheel(
    UINodeId node_id,
    std::function<void(const UIMouseWheelInputEvent &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_mouse_wheel = std::move(callback);
  }
}

void UIDocument::set_on_change(
    UINodeId node_id, std::function<void(const std::string &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_change = std::move(callback);
  }
}

void UIDocument::set_on_submit(
    UINodeId node_id, std::function<void(const std::string &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_submit = std::move(callback);
  }
}

void UIDocument::set_on_toggle(UINodeId node_id, std::function<void(bool)> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_toggle = std::move(callback);
  }
}

void UIDocument::set_on_value_change(UINodeId node_id, std::function<void(float)> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_value_change = std::move(callback);
  }
}

void UIDocument::set_on_select(
    UINodeId node_id,
    std::function<void(size_t, const std::string &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_select = std::move(callback);
  }
}

void UIDocument::set_on_chip_toggle(
    UINodeId node_id,
    std::function<void(size_t, const std::string &, bool)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_chip_toggle = std::move(callback);
  }
}

void UIDocument::set_text_selection(UINodeId node_id, UITextSelection selection) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  target->selection = clamp_text_selection(*target, selection);
  m_paint_dirty = true;
}

void UIDocument::clear_text_selection(UINodeId node_id) {
  set_text_selection(node_id, UITextSelection{});
}

void UIDocument::set_caret(UINodeId node_id, size_t index, bool active) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  target->caret.index = clamp_text_index(*target, index);
  target->caret.active = active;
  target->caret.visible = true;
  target->caret.blink_elapsed = 0.0;
  m_paint_dirty = true;
}

void UIDocument::clear_caret(UINodeId node_id) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  target->caret.active = false;
  target->caret.visible = false;
  target->caret.blink_elapsed = 0.0;
  m_paint_dirty = true;
}

void UIDocument::reset_caret_blink(UINodeId node_id) {
  UINode *target = node(node_id);
  if (target == nullptr || !target->caret.active) {
    return;
  }

  target->caret.visible = true;
  target->caret.blink_elapsed = 0.0;
  m_paint_dirty = true;
}

void UIDocument::set_scroll_offset(UINodeId node_id, glm::vec2 offset) {
  UINode *target = node(node_id);
  if (target == nullptr) {
    return;
  }

  if (target->layout.scroll.offset == offset) {
    return;
  }

  target->layout.scroll.offset = offset;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_canvas_size(glm::vec2 canvas_size) {
  if (m_canvas_size == canvas_size) {
    return;
  }

  m_canvas_size = canvas_size;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::set_root_font_size(float root_font_size) {
  if (m_root_font_size == root_font_size) {
    return;
  }

  m_root_font_size = root_font_size;
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::mark_layout_dirty() {
  m_layout_dirty = true;
  m_paint_dirty = true;
}

void UIDocument::mark_paint_dirty() { m_paint_dirty = true; }

void UIDocument::clear_layout_dirty() {
  m_structure_dirty = false;
  m_layout_dirty = false;
}

void UIDocument::clear_paint_dirty() { m_paint_dirty = false; }

void UIDocument::clear_dirty() {
  m_structure_dirty = false;
  m_layout_dirty = false;
  m_paint_dirty = false;
}

void UIDocument::queue_callback(const std::function<void()> &callback) {
  if (callback) {
    m_callback_queue.push_back(callback);
  }
}

void UIDocument::flush_callbacks() {
  if (m_callback_queue.empty()) {
    return;
  }

  auto callbacks = std::move(m_callback_queue);
  m_callback_queue.clear();

  for (auto &callback : callbacks) {
    if (callback) {
      callback();
    }
  }
}

void UIDocument::set_hot_node(UINodeId node_id) {
  if (m_hot_node == node_id) {
    return;
  }

  if (UINode *previous = node(m_hot_node); previous != nullptr) {
    previous->paint_state.hovered = false;
  }

  m_hot_node = node_id;

  if (UINode *current = node(m_hot_node); current != nullptr) {
    current->paint_state.hovered = true;
  }

  m_paint_dirty = true;
}

void UIDocument::set_active_node(UINodeId node_id) {
  if (m_active_node == node_id) {
    return;
  }

  if (UINode *previous = node(m_active_node); previous != nullptr) {
    previous->paint_state.pressed = false;
  }

  m_active_node = node_id;

  if (UINode *current = node(m_active_node); current != nullptr) {
    current->paint_state.pressed = true;
  }

  m_paint_dirty = true;
}

void UIDocument::set_focused_node(UINodeId node_id) {
  if (m_focused_node == node_id) {
    return;
  }

  if (m_open_select_node != k_invalid_node_id && m_open_select_node != node_id) {
    set_select_open(m_open_select_node, false);
  }

  if (UINode *previous = node(m_focused_node); previous != nullptr) {
    previous->paint_state.focused = false;
  }

  m_focused_node = node_id;

  if (UINode *current = node(m_focused_node); current != nullptr) {
    current->paint_state.focused = true;
  }

  m_paint_dirty = true;
}

void UIDocument::clear_focus() { set_focused_node(k_invalid_node_id); }

void UIDocument::request_focus(UINodeId node_id) {
  if (node_id != k_invalid_node_id && !is_valid_node(node_id)) {
    return;
  }

  m_requested_focus_node = node_id;
}

UINodeId UIDocument::consume_requested_focus() {
  const UINodeId requested = m_requested_focus_node;
  m_requested_focus_node = k_invalid_node_id;
  return requested;
}

void UIDocument::suppress_next_character_input(uint32_t codepoint) {
  m_suppressed_character_input_codepoint = codepoint;
}

bool UIDocument::consume_suppressed_character_input(uint32_t codepoint) {
  if (!m_suppressed_character_input_codepoint.has_value() ||
      *m_suppressed_character_input_codepoint != codepoint) {
    return false;
  }

  m_suppressed_character_input_codepoint.reset();
  return true;
}

UINodeId UIDocument::parent(UINodeId node_id) const {
  const UINode *target = node(node_id);
  return target != nullptr ? target->parent : k_invalid_node_id;
}

UIScrollState *UIDocument::scroll_state(UINodeId node_id) {
  UINode *target = node(node_id);
  return target != nullptr ? &target->layout.scroll : nullptr;
}

const UIScrollState *UIDocument::scroll_state(UINodeId node_id) const {
  const UINode *target = node(node_id);
  return target != nullptr ? &target->layout.scroll : nullptr;
}

std::vector<UINodeId> UIDocument::root_to_leaf_order() const {
  std::vector<UINodeId> order;
  if (!is_valid_node(m_root_id)) {
    return order;
  }

  std::vector<UINodeId> stack{m_root_id};
  while (!stack.empty()) {
    const UINodeId node_id = stack.back();
    stack.pop_back();
    order.push_back(node_id);

    const UINode *current = node(node_id);
    if (current == nullptr) {
      continue;
    }

    for (auto it = current->children.rbegin(); it != current->children.rend();
         ++it) {
      stack.push_back(*it);
    }
  }

  return order;
}

} // namespace astralix::ui
