#include "document/document.hpp"
#include "immediate/runtime.hpp"

#include <gtest/gtest.h>

namespace astralix::ui::im {
namespace {

TEST(UIFoundationsTest, ImmediateRuntimePreservesKeyedNodeIdentityAcrossReorder) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId row_one_field = k_invalid_widget_id;
  WidgetId row_two_field = k_invalid_widget_id;
  WidgetId list_widget = k_invalid_widget_id;
  auto render_rows = [&](int first_key, int second_key) {
    runtime.render([&](Frame &ui) {
      auto list = ui.column("list");
      list_widget = list.widget_id();

      auto first_scope = list.item_scope("row", first_key);
      const WidgetId first_widget_id =
          first_scope.text_input("field", std::to_string(first_key)).widget_id();
      if (first_key == 1) {
        row_one_field = first_widget_id;
      } else {
        row_two_field = first_widget_id;
      }

      auto second_scope = list.item_scope("row", second_key);
      const WidgetId second_widget_id =
          second_scope.text_input("field", std::to_string(second_key)).widget_id();
      if (second_key == 1) {
        row_one_field = second_widget_id;
      } else {
        row_two_field = second_widget_id;
      }
    });
  };

  render_rows(1, 2);

  const UINodeId first_node = runtime.node_id_for(row_one_field);
  const UINodeId second_node = runtime.node_id_for(row_two_field);
  ASSERT_NE(first_node, k_invalid_node_id);
  ASSERT_NE(second_node, k_invalid_node_id);

  document->set_text_selection(
      second_node,
      UITextSelection{
          .anchor = 0u,
          .focus = 1u,
      }
  );

  render_rows(2, 1);

  EXPECT_EQ(runtime.node_id_for(row_one_field), first_node);
  EXPECT_EQ(runtime.node_id_for(row_two_field), second_node);

  const auto *list_node = document->node(runtime.node_id_for(list_widget));
  ASSERT_NE(list_node, nullptr);
  ASSERT_EQ(list_node->children.size(), 2u);
  EXPECT_EQ(list_node->children[0], second_node);
  EXPECT_EQ(list_node->children[1], first_node);

  const auto *preserved_second = document->node(second_node);
  ASSERT_NE(preserved_second, nullptr);
  EXPECT_EQ(preserved_second->selection.anchor, 0u);
  EXPECT_EQ(preserved_second->selection.focus, 1u);
}

TEST(UIFoundationsTest, ImmediateRuntimeDestroysRemovedSubtreesAndClearsDocumentRefs) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId input_widget = k_invalid_widget_id;
  WidgetId anchor_widget = k_invalid_widget_id;
  WidgetId popover_widget = k_invalid_widget_id;

  runtime.render([&](Frame &ui) {
    input_widget = ui.text_input("input", "hello").widget_id();
    anchor_widget = ui.button("anchor", "Open").widget_id();
    popover_widget =
        static_cast<Children &>(ui)
            .popover("menu")
            .popover(PopoverState{
                .open = true,
                .anchor_widget_id = anchor_widget,
                .placement = UIPopupPlacement::BottomStart,
                .depth = 0u,
            })
            .widget_id();
  });

  const UINodeId input_node = runtime.node_id_for(input_widget);
  const UINodeId popover_node = runtime.node_id_for(popover_widget);
  ASSERT_NE(input_node, k_invalid_node_id);
  ASSERT_NE(popover_node, k_invalid_node_id);
  ASSERT_FALSE(document->open_popover_stack().empty());
  EXPECT_EQ(document->open_popover_stack().back(), popover_node);

  document->set_focused_node(input_node);
  ASSERT_EQ(document->focused_node(), input_node);

  runtime.render([&](Frame &ui) {
    ui.text("empty", "nothing to keep");
  });

  EXPECT_EQ(document->node(input_node), nullptr);
  EXPECT_EQ(document->node(popover_node), nullptr);
  EXPECT_EQ(document->focused_node(), k_invalid_node_id);
  EXPECT_EQ(document->open_popup_node(), k_invalid_node_id);
  EXPECT_TRUE(document->open_popover_stack().empty());
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesTextAndPlaceholderToTextInput) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId input_widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    input_widget = ui.text_input("field", "hello", "type here").widget_id();
  });

  const UINodeId node_id = runtime.node_id_for(input_widget);
  ASSERT_NE(node_id, k_invalid_node_id);

  const auto *node = document->node(node_id);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->text, "hello");
  EXPECT_EQ(node->placeholder, "type here");
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesVisibleAndEnabledFlags) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.view("box").visible(false).enabled(false).widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  EXPECT_FALSE(node->visible);
  EXPECT_FALSE(node->enabled);

  runtime.render([&](Frame &ui) {
    widget = ui.view("box").visible(true).enabled(true).widget_id();
  });

  const auto *updated = document->node(runtime.node_id_for(widget));
  ASSERT_NE(updated, nullptr);
  EXPECT_TRUE(updated->visible);
  EXPECT_TRUE(updated->enabled);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesFocusableAndReadOnly) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.text_input("field", "value")
                 .focusable(true)
                 .read_only(true)
                 .select_all_on_focus(true)
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  EXPECT_TRUE(node->focusable);
  EXPECT_TRUE(node->read_only);
  EXPECT_TRUE(node->select_all_on_focus);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesCheckboxCheckedState) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.checkbox("toggle", "Accept", true).widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  EXPECT_TRUE(node->checkbox.checked);

  runtime.render([&](Frame &ui) {
    widget = ui.checkbox("toggle", "Accept", false).widget_id();
  });

  const auto *unchecked = document->node(runtime.node_id_for(widget));
  ASSERT_NE(unchecked, nullptr);
  EXPECT_FALSE(unchecked->checkbox.checked);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesSliderState) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.slider("volume", 0.75f, 0.0f, 1.0f)
                 .step(0.05f)
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  EXPECT_FLOAT_EQ(node->slider.value, 0.75f);
  EXPECT_FLOAT_EQ(node->slider.min_value, 0.0f);
  EXPECT_FLOAT_EQ(node->slider.max_value, 1.0f);
  EXPECT_FLOAT_EQ(node->slider.step, 0.05f);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesSelectOptionsAndIndex) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.select("color", {"Red", "Green", "Blue"}, 1u).widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->select.options.size(), 3u);
  EXPECT_EQ(node->select.options[0], "Red");
  EXPECT_EQ(node->select.options[1], "Green");
  EXPECT_EQ(node->select.options[2], "Blue");
  EXPECT_EQ(node->select.selected_index, 1u);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesSelectHighlightedIndex) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.select("picker", {"A", "B", "C"}, 0u)
                 .highlighted_index(2u)
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->select.highlighted_index, 2u);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesComboboxState) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.combobox("search", "query", "Search...")
                 .options({"Option A", "Option B"})
                 .combobox_open(true)
                 .highlighted_index(1u)
                 .open_on_arrow_keys(false)
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->combobox.options.size(), 2u);
  EXPECT_EQ(node->combobox.options[0], "Option A");
  EXPECT_TRUE(node->combobox.open);
  EXPECT_EQ(node->combobox.highlighted_index, 1u);
  EXPECT_FALSE(node->combobox.open_on_arrow_keys);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesSegmentedControlState) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.segmented_control("tabs", {"Tab A", "Tab B", "Tab C"}, 2u)
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->segmented_control.options.size(), 3u);
  EXPECT_EQ(node->segmented_control.selected_index, 2u);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesChipGroupState) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.chip_group("tags", {"Fast", "Cheap", "Good"}, {true, false, true})
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->chip_group.options.size(), 3u);
  EXPECT_EQ(node->chip_group.options[0], "Fast");
  ASSERT_EQ(node->chip_group.selected.size(), 3u);
  EXPECT_TRUE(node->chip_group.selected[0]);
  EXPECT_FALSE(node->chip_group.selected[1]);
  EXPECT_TRUE(node->chip_group.selected[2]);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesPopoverOpenWithAnchorWidget) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId anchor_widget = k_invalid_widget_id;
  WidgetId popover_widget = k_invalid_widget_id;

  runtime.render([&](Frame &ui) {
    anchor_widget = ui.button("trigger", "Open").widget_id();
    popover_widget =
        static_cast<Children &>(ui)
            .popover("dropdown")
            .popover(PopoverState{
                .open = true,
                .anchor_widget_id = anchor_widget,
                .placement = UIPopupPlacement::BottomStart,
                .depth = 0u,
            })
            .widget_id();
  });

  ASSERT_FALSE(document->open_popover_stack().empty());
  const UINodeId popover_node = runtime.node_id_for(popover_widget);
  EXPECT_EQ(document->open_popover_stack().back(), popover_node);

  const auto *node = document->node(popover_node);
  ASSERT_NE(node, nullptr);
  EXPECT_TRUE(node->popover.open);
  EXPECT_TRUE(node->popover.close_on_escape);
  EXPECT_TRUE(node->popover.close_on_outside_click);
}

TEST(UIFoundationsTest, ImmediateRuntimeClosesPopoverWhenOpenIsFalse) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId anchor_widget = k_invalid_widget_id;
  WidgetId popover_widget = k_invalid_widget_id;

  runtime.render([&](Frame &ui) {
    anchor_widget = ui.button("trigger", "Open").widget_id();
    popover_widget =
        static_cast<Children &>(ui)
            .popover("dropdown")
            .popover(PopoverState{
                .open = true,
                .anchor_widget_id = anchor_widget,
                .placement = UIPopupPlacement::BottomStart,
                .depth = 0u,
            })
            .widget_id();
  });

  ASSERT_FALSE(document->open_popover_stack().empty());

  runtime.render([&](Frame &ui) {
    anchor_widget = ui.button("trigger", "Open").widget_id();
    popover_widget =
        static_cast<Children &>(ui)
            .popover("dropdown")
            .popover(PopoverState{
                .open = false,
                .anchor_widget_id = anchor_widget,
            })
            .widget_id();
  });

  EXPECT_TRUE(document->open_popover_stack().empty());
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesPopoverCloseOnEscapeAndOutsideClick) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId anchor_widget = k_invalid_widget_id;
  WidgetId popover_widget = k_invalid_widget_id;

  runtime.render([&](Frame &ui) {
    anchor_widget = ui.button("trigger", "Open").widget_id();
    popover_widget =
        static_cast<Children &>(ui)
            .popover("dropdown")
            .popover(PopoverState{
                .open = true,
                .anchor_widget_id = anchor_widget,
                .placement = UIPopupPlacement::BottomStart,
                .depth = 0u,
                .close_on_outside_click = false,
                .close_on_escape = false,
            })
            .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(popover_widget));
  ASSERT_NE(node, nullptr);
  EXPECT_FALSE(node->popover.close_on_escape);
  EXPECT_FALSE(node->popover.close_on_outside_click);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesLineChartState) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  const std::vector<UILineChartSeries> series = {
      UILineChartSeries{
          .values = {1.0f, 2.0f, 3.0f},
          .color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
          .thickness = 2.0f,
      },
  };

  runtime.render([&](Frame &ui) {
    widget = ui.line_chart("chart")
                 .line_chart_series(series)
                 .line_chart_auto_range(false)
                 .line_chart_range(0.0f, 10.0f)
                 .line_chart_grid(8u, glm::vec4(0.5f, 0.5f, 0.5f, 0.2f))
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->line_chart.series.size(), 1u);
  EXPECT_EQ(node->line_chart.series[0].values.size(), 3u);
  EXPECT_FALSE(node->line_chart.auto_range);
  EXPECT_FLOAT_EQ(node->line_chart.y_min, 0.0f);
  EXPECT_FLOAT_EQ(node->line_chart.y_max, 10.0f);
  EXPECT_EQ(node->line_chart.grid_line_count, 8u);
  EXPECT_FLOAT_EQ(node->line_chart.grid_color.r, 0.5f);
}

TEST(UIFoundationsTest, ImmediateRuntimeForwardsClickCallback) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  bool clicked = false;
  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.button("btn", "Click me", [&] { clicked = true; }).widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  ASSERT_TRUE(node->on_click);

  node->on_click();
  EXPECT_TRUE(clicked);
}

TEST(UIFoundationsTest, ImmediateRuntimeForwardsOnChangeCallback) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  std::string captured_value;
  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.text_input("field", "")
                 .on_change([&](const std::string &value) { captured_value = value; })
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  ASSERT_TRUE(node->on_change);

  node->on_change("updated");
  EXPECT_EQ(captured_value, "updated");
}

TEST(UIFoundationsTest, ImmediateRuntimeForwardsOnToggleCallback) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  bool toggled_value = false;
  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.checkbox("check", "Toggle")
                 .on_toggle([&](bool value) { toggled_value = value; })
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  ASSERT_TRUE(node->on_toggle);

  node->on_toggle(true);
  EXPECT_TRUE(toggled_value);
}

TEST(UIFoundationsTest, ImmediateRuntimeForwardsOnSelectCallback) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  size_t selected_index = 0u;
  std::string selected_value;
  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.select("picker", {"A", "B", "C"}, 0u)
                 .on_select([&](size_t index, const std::string &value) {
                   selected_index = index;
                   selected_value = value;
                 })
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  ASSERT_TRUE(node->on_select);

  node->on_select(2u, "C");
  EXPECT_EQ(selected_index, 2u);
  EXPECT_EQ(selected_value, "C");
}

TEST(UIFoundationsTest, ImmediateRuntimeForwardsOnValueChangeCallback) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  float changed_value = 0.0f;
  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.slider("vol", 0.5f)
                 .on_value_change([&](float value) { changed_value = value; })
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  ASSERT_TRUE(node->on_value_change);

  node->on_value_change(0.8f);
  EXPECT_FLOAT_EQ(changed_value, 0.8f);
}

TEST(UIFoundationsTest, ImmediateRuntimeClearsCallbackOnRerender) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  bool clicked = false;
  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.button("btn", "Click", [&] { clicked = true; }).widget_id();
  });

  const UINodeId node_id = runtime.node_id_for(widget);
  const auto *node = document->node(node_id);
  ASSERT_NE(node, nullptr);
  ASSERT_TRUE(node->on_click);
  node->on_click();
  EXPECT_TRUE(clicked);

  clicked = false;
  runtime.render([&](Frame &ui) {
    widget = ui.button("btn", "Click").widget_id();
  });

  node = document->node(node_id);
  ASSERT_NE(node, nullptr);
  node->on_click();
  EXPECT_FALSE(clicked);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesFocusRequest) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId input_widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    input_widget = ui.text_input("field", "").focusable(true).widget_id();
  });

  runtime.render([&](Frame &ui) {
    input_widget = ui.text_input("field", "").focusable(true).widget_id();
    ui.request_focus(input_widget);
  });

  EXPECT_EQ(document->focused_node(), runtime.node_id_for(input_widget));
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesCaretRequest) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId input_widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    input_widget = ui.text_input("field", "hello").focusable(true).widget_id();
    ui.set_caret(input_widget, 3u, true);
  });

  const auto *node = document->node(runtime.node_id_for(input_widget));
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->caret.index, 3u);
  EXPECT_TRUE(node->caret.active);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesTextSelectionRequest) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId input_widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    input_widget = ui.text_input("field", "hello world").focusable(true).widget_id();
    ui.set_text_selection(input_widget, UITextSelection{.anchor = 0u, .focus = 5u});
  });

  const auto *node = document->node(runtime.node_id_for(input_widget));
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->selection.anchor, 0u);
  EXPECT_EQ(node->selection.focus, 5u);
}

TEST(UIFoundationsTest, ImmediateRuntimeReconcilesMixedWidgetTree) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId container_widget = k_invalid_widget_id;
  WidgetId text_widget = k_invalid_widget_id;
  WidgetId button_widget = k_invalid_widget_id;
  WidgetId input_widget = k_invalid_widget_id;

  runtime.render([&](Frame &ui) {
    auto container = ui.column("main");
    container_widget = container.widget_id();
    text_widget = container.text("title", "Hello").widget_id();
    button_widget = container.button("action", "Submit").widget_id();
    input_widget = container.text_input("name", "value").widget_id();
  });

  const auto *container = document->node(runtime.node_id_for(container_widget));
  ASSERT_NE(container, nullptr);
  ASSERT_EQ(container->children.size(), 3u);
  EXPECT_EQ(container->children[0], runtime.node_id_for(text_widget));
  EXPECT_EQ(container->children[1], runtime.node_id_for(button_widget));
  EXPECT_EQ(container->children[2], runtime.node_id_for(input_widget));
}

TEST(UIFoundationsTest, ImmediateRuntimeAddsAndRemovesChildrenAcrossRerenders) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId container_widget = k_invalid_widget_id;

  runtime.render([&](Frame &ui) {
    auto container = ui.column("list");
    container_widget = container.widget_id();
    container.text("a", "Item A");
    container.text("b", "Item B");
    container.text("c", "Item C");
  });

  const auto *container = document->node(runtime.node_id_for(container_widget));
  ASSERT_NE(container, nullptr);
  EXPECT_EQ(container->children.size(), 3u);

  runtime.render([&](Frame &ui) {
    auto list = ui.column("list");
    list.text("a", "Item A");
  });

  container = document->node(runtime.node_id_for(container_widget));
  ASSERT_NE(container, nullptr);
  EXPECT_EQ(container->children.size(), 1u);

  runtime.render([&](Frame &ui) {
    auto list = ui.column("list");
    list.text("a", "Item A");
    list.text("d", "Item D");
    list.text("e", "Item E");
    list.text("f", "Item F");
  });

  container = document->node(runtime.node_id_for(container_widget));
  ASSERT_NE(container, nullptr);
  EXPECT_EQ(container->children.size(), 4u);
}

TEST(UIFoundationsTest, ImmediateRuntimeNodeIdForReturnsInvalidForUnknownWidget) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  runtime.render([&](Frame &ui) {
    ui.text("label", "text");
  });

  EXPECT_EQ(runtime.node_id_for(WidgetId{999999u}), k_invalid_node_id);
}

TEST(UIFoundationsTest, ImmediateRuntimeUpdatesTextAcrossRerenders) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.text_input("field", "first").widget_id();
  });

  const UINodeId node_id = runtime.node_id_for(widget);
  EXPECT_EQ(document->node(node_id)->text, "first");

  runtime.render([&](Frame &ui) {
    widget = ui.text_input("field", "second").widget_id();
  });

  EXPECT_EQ(runtime.node_id_for(widget), node_id);
  EXPECT_EQ(document->node(node_id)->text, "second");
}

TEST(UIFoundationsTest, ImmediateRuntimeUpdatesSliderValueAcrossRerenders) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.slider("volume", 0.2f).widget_id();
  });

  const UINodeId node_id = runtime.node_id_for(widget);
  EXPECT_FLOAT_EQ(document->node(node_id)->slider.value, 0.2f);

  runtime.render([&](Frame &ui) {
    widget = ui.slider("volume", 0.9f).widget_id();
  });

  EXPECT_FLOAT_EQ(document->node(node_id)->slider.value, 0.9f);
}

TEST(UIFoundationsTest, ImmediateRuntimeFrozenNodeSkipsUpdate) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.text_input("field", "initial").widget_id();
  });

  const UINodeId node_id = runtime.node_id_for(widget);
  EXPECT_EQ(document->node(node_id)->text, "initial");

  runtime.render([&](Frame &ui) {
    widget = ui.text_input("field", "should not update").frozen(true).widget_id();
  });

  EXPECT_EQ(document->node(node_id)->text, "initial");
}

TEST(UIFoundationsTest, ImmediateRuntimeReplacesNodeWhenKindChanges) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.text_input("widget", "text").widget_id();
  });

  const UINodeId original_node = runtime.node_id_for(widget);
  ASSERT_NE(original_node, k_invalid_node_id);

  runtime.render([&](Frame &ui) {
    widget = ui.checkbox("widget", "check", false).widget_id();
  });

  const UINodeId replaced_node = runtime.node_id_for(widget);
  ASSERT_NE(replaced_node, k_invalid_node_id);
  EXPECT_NE(replaced_node, original_node);
  EXPECT_EQ(document->node(original_node), nullptr);
}

TEST(UIFoundationsTest, ImmediateRuntimeCallbackUpdatesAcrossRerenders) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  int call_count = 0;
  WidgetId widget = k_invalid_widget_id;

  runtime.render([&](Frame &ui) {
    widget = ui.button("btn", "Click", [&] { call_count = 1; }).widget_id();
  });

  const UINodeId node_id = runtime.node_id_for(widget);
  document->node(node_id)->on_click();
  EXPECT_EQ(call_count, 1);

  runtime.render([&](Frame &ui) {
    widget = ui.button("btn", "Click", [&] { call_count = 2; }).widget_id();
  });

  document->node(node_id)->on_click();
  EXPECT_EQ(call_count, 2);
}

TEST(UIFoundationsTest, ImmediateRuntimeNestedContainerHierarchy) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId outer_widget = k_invalid_widget_id;
  WidgetId inner_widget = k_invalid_widget_id;
  WidgetId leaf_widget = k_invalid_widget_id;

  runtime.render([&](Frame &ui) {
    auto outer = ui.column("outer");
    outer_widget = outer.widget_id();
    auto inner = outer.row("inner");
    inner_widget = inner.widget_id();
    leaf_widget = inner.text("leaf", "deep").widget_id();
  });

  const auto *outer = document->node(runtime.node_id_for(outer_widget));
  ASSERT_NE(outer, nullptr);
  ASSERT_EQ(outer->children.size(), 1u);
  EXPECT_EQ(outer->children[0], runtime.node_id_for(inner_widget));

  const auto *inner = document->node(runtime.node_id_for(inner_widget));
  ASSERT_NE(inner, nullptr);
  ASSERT_EQ(inner->children.size(), 1u);
  EXPECT_EQ(inner->children[0], runtime.node_id_for(leaf_widget));
}

TEST(UIFoundationsTest, ImmediateRuntimeSelectOpenState) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.select("dropdown", {"A", "B"}, 0u)
                 .select_open(true)
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  EXPECT_TRUE(node->select.open);
}

TEST(UIFoundationsTest, ImmediateRuntimeMultipleRerendersCyclePreservesStability) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  UINodeId stable_node = k_invalid_node_id;

  for (int iteration = 0; iteration < 10; ++iteration) {
    runtime.render([&](Frame &ui) {
      widget = ui.text_input("field", std::to_string(iteration)).widget_id();
    });

    const UINodeId current_node = runtime.node_id_for(widget);
    if (iteration == 0) {
      stable_node = current_node;
    }
    EXPECT_EQ(current_node, stable_node);
    EXPECT_EQ(document->node(current_node)->text, std::to_string(iteration));
  }
}

} // namespace
} // namespace astralix::ui::im
