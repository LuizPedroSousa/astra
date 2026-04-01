#include "document/document.hpp"
#include "foundations.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, TextInputAndComboboxExposeExpectedDefaults) {
  auto document = UIDocument::create();
  const UINodeId text_input = document->create_text_input("seed", "Placeholder");
  const UINodeId combobox = document->create_combobox("help", "Command");

  document->set_placeholder(text_input, "Updated placeholder");
  document->set_autocomplete_text(text_input, "changed by history");
  document->set_read_only(text_input, true);
  document->set_select_all_on_focus(text_input, true);
  document->set_combobox_options(combobox, {"help", "hello"});
  document->set_combobox_highlighted_index(combobox, 5u);
  document->set_combobox_open(combobox, true);

  int on_change_calls = 0;
  int on_submit_calls = 0;
  document->set_on_change(text_input, [&](const std::string &) {
    ++on_change_calls;
  });
  document->set_on_submit(text_input, [&](const std::string &) {
    ++on_submit_calls;
  });

  const auto *text_input_node = document->node(text_input);
  const auto *combobox_node = document->node(combobox);
  ASSERT_NE(text_input_node, nullptr);
  ASSERT_NE(combobox_node, nullptr);
  EXPECT_EQ(text_input_node->type, NodeType::TextInput);
  EXPECT_TRUE(text_input_node->focusable);
  EXPECT_TRUE(text_input_node->read_only);
  EXPECT_TRUE(text_input_node->select_all_on_focus);
  EXPECT_EQ(text_input_node->placeholder, "Updated placeholder");
  EXPECT_EQ(text_input_node->autocomplete_text, "changed by history");
  EXPECT_EQ(text_input_node->style.overflow, Overflow::Hidden);
  EXPECT_FLOAT_EQ(text_input_node->style.flex_shrink, 0.0f);
  EXPECT_FALSE(text_input_node->style.focused_style.border_color.has_value());
  EXPECT_FALSE(text_input_node->style.focused_style.border_width.has_value());
  EXPECT_FALSE(text_input_node->style.hovered_style.border_color.has_value());
  EXPECT_TRUE(text_input_node->style.disabled_style.opacity.has_value());

  EXPECT_EQ(combobox_node->type, NodeType::Combobox);
  EXPECT_TRUE(combobox_node->focusable);
  EXPECT_EQ(combobox_node->text, "help");
  EXPECT_EQ(combobox_node->placeholder, "Command");
  ASSERT_EQ(combobox_node->combobox.options.size(), 2u);
  EXPECT_EQ(combobox_node->combobox.highlighted_index, 1u);
  EXPECT_TRUE(combobox_node->combobox.open);
  EXPECT_TRUE(combobox_node->combobox.open_on_arrow_keys);
  EXPECT_EQ(document->open_combobox_node(), combobox);
  EXPECT_TRUE(combobox_node->style.focused_style.border_color.has_value());

  document->set_text(text_input, "changed by code");
  document->flush_callbacks();
  EXPECT_EQ(on_change_calls, 0);
  EXPECT_EQ(on_submit_calls, 0);
}

TEST(UIFoundationsTest, CheckboxSliderAndSelectExposeExpectedDefaults) {
  auto document = UIDocument::create();
  const UINodeId checkbox = document->create_checkbox("Follow tail", true);
  const UINodeId slider = document->create_slider(0.24f, 0.0f, 1.0f, 0.2f);
  const UINodeId select =
      document->create_select({"All", "Info", "Warn"}, 7u, "Severity");
  const UINodeId second_select =
      document->create_select({"A", "B"}, 0u, "Other");

  bool toggled = false;
  float changed_value = 0.0f;
  size_t selected = 0u;
  std::string selected_label;
  document->set_on_toggle(checkbox, [&](bool value) { toggled = value; });
  document->set_on_value_change(slider, [&](float value) {
    changed_value = value;
  });
  document->set_on_select(select, [&](size_t index, const std::string &label) {
    selected = index;
    selected_label = label;
  });

  const auto *checkbox_node = document->node(checkbox);
  ASSERT_NE(checkbox_node, nullptr);
  EXPECT_EQ(checkbox_node->type, NodeType::Checkbox);
  EXPECT_TRUE(checkbox_node->focusable);
  EXPECT_TRUE(checkbox_node->checkbox.checked);
  ASSERT_TRUE(checkbox_node->style.cursor.has_value());
  EXPECT_EQ(*checkbox_node->style.cursor, CursorStyle::Pointer);
  EXPECT_TRUE(checkbox_node->style.hovered_style.background_color.has_value());
  EXPECT_TRUE(checkbox_node->style.focused_style.border_color.has_value());
  EXPECT_TRUE(checkbox_node->style.disabled_style.opacity.has_value());
  EXPECT_TRUE(static_cast<bool>(checkbox_node->on_toggle));

  const auto *slider_node = document->node(slider);
  ASSERT_NE(slider_node, nullptr);
  EXPECT_EQ(slider_node->type, NodeType::Slider);
  EXPECT_TRUE(slider_node->focusable);
  EXPECT_FLOAT_EQ(slider_node->slider.value, 0.2f);
  EXPECT_FLOAT_EQ(slider_node->slider.step, 0.2f);
  ASSERT_TRUE(slider_node->style.cursor.has_value());
  EXPECT_EQ(*slider_node->style.cursor, CursorStyle::Pointer);
  EXPECT_TRUE(slider_node->style.focused_style.border_color.has_value());
  EXPECT_TRUE(slider_node->style.disabled_style.opacity.has_value());
  EXPECT_TRUE(static_cast<bool>(slider_node->on_value_change));

  const auto *select_node = document->node(select);
  ASSERT_NE(select_node, nullptr);
  EXPECT_EQ(select_node->type, NodeType::Select);
  EXPECT_TRUE(select_node->focusable);
  EXPECT_EQ(select_node->select.selected_index, 2u);
  EXPECT_EQ(select_node->select.highlighted_index, 2u);
  EXPECT_EQ(select_node->placeholder, "Severity");
  ASSERT_TRUE(select_node->style.cursor.has_value());
  EXPECT_EQ(*select_node->style.cursor, CursorStyle::Pointer);
  EXPECT_TRUE(select_node->style.focused_style.border_color.has_value());
  EXPECT_TRUE(select_node->style.disabled_style.opacity.has_value());
  EXPECT_TRUE(static_cast<bool>(select_node->on_select));

  document->set_checked(checkbox, false);
  EXPECT_FALSE(document->checked(checkbox));

  document->set_slider_range(slider, 0.0f, 10.0f, 2.0f);
  document->set_slider_value(slider, 7.1f);
  EXPECT_FLOAT_EQ(document->slider_value(slider), 8.0f);

  document->set_selected_index(select, 1u);
  EXPECT_EQ(document->selected_index(select), 1u);

  document->set_select_open(select, true);
  EXPECT_TRUE(document->select_open(select));
  EXPECT_EQ(document->open_select_node(), select);

  document->set_select_open(second_select, true);
  EXPECT_FALSE(document->select_open(select));
  EXPECT_TRUE(document->select_open(second_select));
  EXPECT_EQ(document->open_select_node(), second_select);

  const UINodeId combobox = document->create_combobox("help", "Command");
  document->set_combobox_options(combobox, {"help", "hello"});
  document->set_combobox_open(combobox, true);
  EXPECT_TRUE(document->combobox_open(combobox));
  EXPECT_EQ(document->open_combobox_node(), combobox);
  EXPECT_FALSE(document->select_open(second_select));

  document->set_select_options(select, {});
  EXPECT_FALSE(document->select_open(select));
  const auto *options = document->select_options(select);
  ASSERT_NE(options, nullptr);
  EXPECT_TRUE(options->empty());

  document->set_combobox_options(combobox, {});
  EXPECT_FALSE(document->combobox_open(combobox));
  const auto *combobox_options = document->combobox_options(combobox);
  ASSERT_NE(combobox_options, nullptr);
  EXPECT_TRUE(combobox_options->empty());

  EXPECT_FALSE(toggled);
  EXPECT_FLOAT_EQ(changed_value, 0.0f);
  EXPECT_EQ(selected, 0u);
  EXPECT_TRUE(selected_label.empty());
}

TEST(UIFoundationsTest, TextInputMutationsClampCaretSelectionAndScrollHelpers) {
  auto document = UIDocument::create();
  const UINodeId text_input = document->create_text_input("hello", "Placeholder");

  document->set_text_selection(
      text_input, UITextSelection{.anchor = 4u, .focus = 5u}
  );
  document->set_caret(text_input, 5u, true);

  auto *node = document->node(text_input);
  ASSERT_NE(node, nullptr);
  node->text_scroll_x = -18.0f;

  document->set_text(text_input, "hi");

  node = document->node(text_input);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->selection.start(), 2u);
  EXPECT_EQ(node->selection.end(), 2u);
  EXPECT_EQ(node->caret.index, 2u);
  EXPECT_FLOAT_EQ(node->text_scroll_x, 0.0f);

  EXPECT_FLOAT_EQ(clamp_text_scroll_x(80.0f, 120.0f, 48.0f), 72.0f);
  EXPECT_FLOAT_EQ(clamp_text_scroll_x(-5.0f, 120.0f, 48.0f), 0.0f);
  EXPECT_FLOAT_EQ(
      scroll_x_to_keep_range_visible(0.0f, 52.0f, 60.0f, 120.0f, 32.0f),
      28.0f
  );
  EXPECT_FLOAT_EQ(
      scroll_x_to_keep_range_visible(24.0f, 8.0f, 16.0f, 120.0f, 32.0f),
      8.0f
  );
}

} // namespace
} // namespace astralix::ui
