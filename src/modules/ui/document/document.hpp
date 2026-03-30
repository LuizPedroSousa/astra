#pragma once

#include "base.hpp"
#include "types.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace astralix::ui {

class UIDocument {
public:
  struct UINode {
    UINodeId id = k_invalid_node_id;
    NodeType type = NodeType::View;
    UINodeId parent = k_invalid_node_id;
    std::vector<UINodeId> children;
    UIStyle style;
    UILayoutMetrics layout;
    UIPaintState paint_state;
    std::string name;
    std::string text;
    std::string placeholder;
    std::string autocomplete_text;
    ResourceDescriptorID texture_id;
    std::optional<RenderImageExportKey> render_image_key;
    bool visible = true;
    bool enabled = true;
    bool focusable = false;
    bool read_only = false;
    bool select_all_on_focus = false;
    float text_scroll_x = 0.0f;
    UITextSelection selection;
    UICaretState caret;
    UICheckboxState checkbox;
    UISliderState slider;
    UISelectState select;
    UIComboboxState combobox;
    UISegmentedControlState segmented_control;
    UIChipGroupState chip_group;

    std::function<void()> on_hover;
    std::function<void()> on_press;
    std::function<void()> on_release;
    std::function<void()> on_click;
    std::function<void()> on_focus;
    std::function<void()> on_blur;
    std::function<void(const UIKeyInputEvent &)> on_key_input;
    std::function<void(const UICharacterInputEvent &)> on_character_input;
    std::function<void(const UIMouseWheelInputEvent &)> on_mouse_wheel;
    std::function<void(const std::string &)> on_change;
    std::function<void(const std::string &)> on_submit;
    std::function<void(bool)> on_toggle;
    std::function<void(float)> on_value_change;
    std::function<void(size_t, const std::string &)> on_select;
    std::function<void(size_t, const std::string &, bool)> on_chip_toggle;
  };

  static Ref<UIDocument> create();

  UINodeId create_view(std::string name = {});
  UINodeId create_text(std::string text = {}, std::string name = {});
  UINodeId create_image(ResourceDescriptorID texture_id = {}, std::string name = {});
  UINodeId create_render_image_view(
      RenderImageExportKey render_image_key, std::string name = {}
  );
  UINodeId create_pressable(std::string name = {});
  UINodeId create_icon_button(ResourceDescriptorID texture_id = {}, const std::function<void()> &on_click = {}, std::string name = {});
  UINodeId create_segmented_control(std::vector<std::string> options = {}, size_t selected_index = 0u, std::string name = {});
  UINodeId create_chip_group(std::vector<std::string> options = {}, std::vector<bool> selected = {}, std::string name = {});
  UINodeId create_text_input(std::string value = {}, std::string placeholder = {}, std::string name = {});
  UINodeId create_combobox(std::string value = {}, std::string placeholder = {}, std::string name = {});
  UINodeId create_scroll_view(std::string name = {});
  UINodeId create_splitter(std::string name = {});
  UINodeId create_checkbox(std::string label = {}, bool checked = false, std::string name = {});
  UINodeId create_slider(float value = 0.0f, float min_value = 0.0f, float max_value = 1.0f, float step = 0.1f, std::string name = {});
  UINodeId create_select(std::vector<std::string> options = {}, size_t selected_index = 0u, std::string placeholder = {}, std::string name = {});
  UINodeId create_button(const std::string &label, const std::function<void()> &on_click, std::string name = {});

  void set_root(UINodeId root_id);
  UINodeId root() const { return m_root_id; }

  void append_child(UINodeId parent_id, UINodeId child_id);
  void remove_child(UINodeId child_id);
  void clear_children(UINodeId parent_id);

  UINode *node(UINodeId node_id);
  const UINode *node(UINodeId node_id) const;

  void set_style(UINodeId node_id, const UIStyle &style);
  void mutate_style(UINodeId node_id, const std::function<void(UIStyle &)> &fn);
  void set_text(UINodeId node_id, std::string text);
  void set_texture(UINodeId node_id, ResourceDescriptorID texture_id);
  void set_render_image_key(
      UINodeId node_id, RenderImageExportKey render_image_key
  );
  void set_placeholder(UINodeId node_id, std::string placeholder);
  void set_autocomplete_text(UINodeId node_id, std::string autocomplete_text);
  void set_visible(UINodeId node_id, bool visible);
  void set_enabled(UINodeId node_id, bool enabled);
  void set_focusable(UINodeId node_id, bool focusable);
  void set_read_only(UINodeId node_id, bool read_only);
  void set_select_all_on_focus(UINodeId node_id, bool select_all_on_focus);
  void set_checked(UINodeId node_id, bool checked);
  bool checked(UINodeId node_id) const;
  void set_slider_value(UINodeId node_id, float value);
  float slider_value(UINodeId node_id) const;
  void set_slider_range(UINodeId node_id, float min_value, float max_value, float step = 0.1f);
  void set_combobox_options(UINodeId node_id, std::vector<std::string> options);
  const std::vector<std::string> *combobox_options(UINodeId node_id) const;
  void set_combobox_open(UINodeId node_id, bool open);
  bool combobox_open(UINodeId node_id) const;
  void set_combobox_highlighted_index(UINodeId node_id, size_t highlighted_index);
  size_t combobox_highlighted_index(UINodeId node_id) const;
  void set_select_options(UINodeId node_id, std::vector<std::string> options);
  const std::vector<std::string> *select_options(UINodeId node_id) const;
  void set_selected_index(UINodeId node_id, size_t selected_index);
  size_t selected_index(UINodeId node_id) const;
  void set_select_open(UINodeId node_id, bool open);
  bool select_open(UINodeId node_id) const;
  void set_segmented_options(UINodeId node_id, std::vector<std::string> options);
  const std::vector<std::string> *segmented_options(UINodeId node_id) const;
  void set_segmented_selected_index(UINodeId node_id, size_t selected_index);
  size_t segmented_selected_index(UINodeId node_id) const;
  void set_chip_options(UINodeId node_id, std::vector<std::string> options, std::vector<bool> selected = {});
  const std::vector<std::string> *chip_options(UINodeId node_id) const;
  void set_chip_selected(UINodeId node_id, size_t index, bool selected);
  bool chip_selected(UINodeId node_id, size_t index) const;
  void set_on_hover(UINodeId node_id, std::function<void()> callback);
  void set_on_press(UINodeId node_id, std::function<void()> callback);
  void set_on_release(UINodeId node_id, std::function<void()> callback);
  void set_on_click(UINodeId node_id, std::function<void()> callback);
  void set_on_focus(UINodeId node_id, std::function<void()> callback);
  void set_on_blur(UINodeId node_id, std::function<void()> callback);
  void set_on_key_input(UINodeId node_id, std::function<void(const UIKeyInputEvent &)> callback);
  void set_on_character_input(
      UINodeId node_id,
      std::function<void(const UICharacterInputEvent &)> callback
  );
  void set_on_mouse_wheel(
      UINodeId node_id,
      std::function<void(const UIMouseWheelInputEvent &)> callback
  );
  void set_on_change(UINodeId node_id, std::function<void(const std::string &)> callback);
  void set_on_submit(UINodeId node_id, std::function<void(const std::string &)> callback);
  void set_on_toggle(UINodeId node_id, std::function<void(bool)> callback);
  void set_on_value_change(UINodeId node_id, std::function<void(float)> callback);
  void set_on_select(
      UINodeId node_id,
      std::function<void(size_t, const std::string &)> callback
  );
  void set_on_chip_toggle(
      UINodeId node_id,
      std::function<void(size_t, const std::string &, bool)> callback
  );
  void set_text_selection(UINodeId node_id, UITextSelection selection);
  void clear_text_selection(UINodeId node_id);
  void set_caret(UINodeId node_id, size_t index, bool active = true);
  void clear_caret(UINodeId node_id);
  void reset_caret_blink(UINodeId node_id);
  void set_scroll_offset(UINodeId node_id, glm::vec2 offset);

  void set_canvas_size(glm::vec2 canvas_size);
  glm::vec2 canvas_size() const { return m_canvas_size; }
  void set_root_font_size(float root_font_size);
  float root_font_size() const { return m_root_font_size; }

  bool layout_dirty() const { return m_layout_dirty; }
  bool paint_dirty() const { return m_paint_dirty || m_layout_dirty; }
  bool structure_dirty() const { return m_structure_dirty; }
  bool dirty() const {
    return m_layout_dirty || m_paint_dirty || m_structure_dirty;
  }

  void mark_layout_dirty();
  void mark_paint_dirty();
  void clear_layout_dirty();
  void clear_paint_dirty();
  void clear_dirty();

  UIDrawList &draw_list() { return m_draw_list; }
  const UIDrawList &draw_list() const { return m_draw_list; }

  void queue_callback(const std::function<void()> &callback);
  void flush_callbacks();

  void set_hot_node(UINodeId node_id);
  void set_active_node(UINodeId node_id);
  void set_focused_node(UINodeId node_id);
  void clear_focus();
  void request_focus(UINodeId node_id);
  UINodeId consume_requested_focus();
  void suppress_next_character_input(uint32_t codepoint);
  bool consume_suppressed_character_input(uint32_t codepoint);
  UINodeId requested_focus() const { return m_requested_focus_node; }
  UINodeId hot_node() const { return m_hot_node; }
  UINodeId active_node() const { return m_active_node; }
  UINodeId focused_node() const { return m_focused_node; }
  UINodeId open_popup_node() const { return m_open_popup_node; }
  UINodeId open_select_node() const;
  UINodeId open_combobox_node() const;

  UINodeId parent(UINodeId node_id) const;
  UIScrollState *scroll_state(UINodeId node_id);
  const UIScrollState *scroll_state(UINodeId node_id) const;

  std::vector<UINodeId> root_to_leaf_order() const;

private:
  struct NodeSlot {
    UINode node;
    bool alive = false;
  };

  UINodeId allocate_node(NodeType type, std::string name);
  void detach_from_parent(UINodeId child_id);
  bool is_valid_node(UINodeId node_id) const;

  std::vector<NodeSlot> m_nodes;
  UINodeId m_root_id = k_invalid_node_id;
  glm::vec2 m_canvas_size = glm::vec2(0.0f);
  float m_root_font_size = 16.0f;

  UIDrawList m_draw_list;
  std::vector<std::function<void()>> m_callback_queue;

  bool m_structure_dirty = true;
  bool m_layout_dirty = true;
  bool m_paint_dirty = true;

  UINodeId m_hot_node = k_invalid_node_id;
  UINodeId m_active_node = k_invalid_node_id;
  UINodeId m_focused_node = k_invalid_node_id;
  UINodeId m_open_popup_node = k_invalid_node_id;
  UINodeId m_requested_focus_node = k_invalid_node_id;
  std::optional<uint32_t> m_suppressed_character_input_codepoint;
};

} // namespace astralix::ui
