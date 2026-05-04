#include "document/document.hpp"
#include "immediate/runtime.hpp"

#include <gtest/gtest.h>

namespace astralix::ui::im {
namespace {

constexpr UIGraphId k_runtime_graph_node_a = 4101u;
constexpr UIGraphId k_runtime_graph_node_b = 4102u;
constexpr UIGraphId k_runtime_graph_port_a_out = 4201u;
constexpr UIGraphId k_runtime_graph_port_b_in = 4202u;
constexpr UIGraphId k_runtime_graph_edge = 4301u;

GraphViewSpec make_runtime_graph_spec(glm::vec2 first_position = glm::vec2(48.0f, 64.0f)) {
  return GraphViewSpec{
      .model =
          UIGraphViewModel{
              .nodes =
                  {
                      UIGraphNode{
                          .id = k_runtime_graph_node_a,
                          .title = "Source",
                          .position = first_position,
                          .output_ports = {k_runtime_graph_port_a_out},
                      },
                      UIGraphNode{
                          .id = k_runtime_graph_node_b,
                          .title = "Sink",
                          .position = glm::vec2(260.0f, 180.0f),
                          .input_ports = {k_runtime_graph_port_b_in},
                      },
                  },
              .ports =
                  {
                      UIGraphPort{
                          .id = k_runtime_graph_port_a_out,
                          .node_id = k_runtime_graph_node_a,
                          .direction = UIGraphPortDirection::Output,
                          .label = "Out",
                          .color = glm::vec4(0.36f, 0.73f, 0.94f, 1.0f),
                      },
                      UIGraphPort{
                          .id = k_runtime_graph_port_b_in,
                          .node_id = k_runtime_graph_node_b,
                          .direction = UIGraphPortDirection::Input,
                          .label = "In",
                          .color = glm::vec4(0.62f, 0.84f, 0.47f, 1.0f),
                      },
                  },
              .edges =
                  {
                      UIGraphEdge{
                          .id = k_runtime_graph_edge,
                          .from_port_id = k_runtime_graph_port_a_out,
                          .to_port_id = k_runtime_graph_port_b_in,
                          .color = glm::vec4(0.62f, 0.82f, 1.0f, 1.0f),
                          .thickness = 2.0f,
                      },
                  },
          },
  };
}

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

TEST(UIFoundationsTest, ImmediateRuntimeAppliesGraphViewStateAndCallbacks) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  bool selection_changed = false;
  bool node_moved = false;
  bool connection_finished = false;
  runtime.render([&](Frame &ui) {
    widget = ui.graph_view("graph", make_runtime_graph_spec())
                 .on_selection_change([&](const UIGraphSelection &selection) {
                   selection_changed = selection.node_ids.size() == 1u &&
                                       selection.node_ids[0] ==
                                           k_runtime_graph_node_b;
                 })
                 .on_node_move([&](UIGraphId node_id, glm::vec2 position) {
                   node_moved = node_id == k_runtime_graph_node_a &&
                                position == glm::vec2(128.0f, 144.0f);
                 })
                 .on_connection_drag_end(
                     [&](UIGraphId from_port_id, std::optional<UIGraphId> to_port_id) {
                       connection_finished =
                           from_port_id == k_runtime_graph_port_a_out &&
                           to_port_id == k_runtime_graph_port_b_in;
                     }
                 )
                 .widget_id();
  });

  const auto selection = runtime.graph_selection(widget);
  ASSERT_TRUE(selection.has_value());
  EXPECT_TRUE(selection->node_ids.empty());
  EXPECT_TRUE(selection->edge_ids.empty());

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->type, NodeType::GraphView);
  ASSERT_EQ(node->graph_view.model.nodes.size(), 2u);
  ASSERT_EQ(node->graph_view.model.ports.size(), 2u);
  ASSERT_EQ(node->graph_view.model.edges.size(), 1u);
  ASSERT_TRUE(static_cast<bool>(node->graph_view.on_selection_change));
  ASSERT_TRUE(static_cast<bool>(node->graph_view.on_node_move));
  ASSERT_TRUE(static_cast<bool>(node->graph_view.on_connection_drag_end));

  node->graph_view.on_selection_change(
      UIGraphSelection{
          .node_ids = {k_runtime_graph_node_b},
      }
  );
  node->graph_view.on_node_move(
      k_runtime_graph_node_a,
      glm::vec2(128.0f, 144.0f)
  );
  node->graph_view.on_connection_drag_end(
      k_runtime_graph_port_a_out,
      k_runtime_graph_port_b_in
  );

  EXPECT_TRUE(selection_changed);
  EXPECT_TRUE(node_moved);
  EXPECT_TRUE(connection_finished);
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

TEST(UIFoundationsTest, ImmediateRuntimeForwardsPointerEventCallback) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  bool received_press = false;
  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.view("surface")
                 .on_pointer_event([&](const UIPointerEvent &event) {
                   received_press =
                       event.phase == UIPointerEventPhase::Press &&
                       event.button == input::MouseButton::Middle;
                 })
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  ASSERT_TRUE(node->on_pointer_event);

  node->on_pointer_event(UIPointerEvent{
      .phase = UIPointerEventPhase::Press,
      .button = input::MouseButton::Middle,
  });
  EXPECT_TRUE(received_press);
}

TEST(UIFoundationsTest, ImmediateRuntimeForwardsCustomHitTestCallback) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  std::optional<glm::vec2> received_local_position;
  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.view("surface")
                 .on_custom_hit_test(
                     [&](glm::vec2 local_position)
                         -> std::optional<UICustomHitData> {
                       received_local_position = local_position;
                       return UICustomHitData{
                           .semantic = 3u,
                           .primary_id = 17u,
                           .secondary_id = 23u,
                       };
                     }
                 )
                 .widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  ASSERT_TRUE(node->on_custom_hit_test);

  auto hit = node->on_custom_hit_test(glm::vec2(8.0f, 12.0f));
  ASSERT_TRUE(hit.has_value());
  ASSERT_TRUE(received_local_position.has_value());
  EXPECT_EQ(*received_local_position, glm::vec2(8.0f, 12.0f));
  EXPECT_EQ(hit->semantic, 3u);
  EXPECT_EQ(hit->primary_id, 17u);
  EXPECT_EQ(hit->secondary_id, 23u);
}

TEST(UIFoundationsTest, ImmediateRuntimeLeavesPointerAndCustomHitCallbacksUnsetByDefault) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.view("surface").widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  EXPECT_FALSE(static_cast<bool>(node->on_pointer_event));
  EXPECT_FALSE(static_cast<bool>(node->on_custom_hit_test));
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

TEST(UIFoundationsTest, ImmediateRuntimeAppliesPointerCaptureRequests) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId canvas_widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    canvas_widget = ui.view("canvas").widget_id();
    ui.request_pointer_capture(canvas_widget, input::MouseButton::Middle);
    ui.release_pointer_capture(canvas_widget, input::MouseButton::Middle);
  });

  const auto requests = document->consume_pointer_capture_requests();
  ASSERT_EQ(requests.size(), 2u);
  EXPECT_EQ(requests[0].node_id, runtime.node_id_for(canvas_widget));
  EXPECT_EQ(requests[0].button, input::MouseButton::Middle);
  EXPECT_EQ(requests[0].action, UIPointerCaptureAction::Capture);
  EXPECT_EQ(requests[1].action, UIPointerCaptureAction::Release);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesViewTransformRequests) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId canvas_widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    auto canvas = ui.view("canvas")
                      .view_transform_enabled()
                      .view_transform_pan_enabled()
                      .view_transform_zoom_enabled();
    canvas_widget = canvas.widget_id();
    ui.set_view_transform(
        canvas_widget,
        UIViewTransform2D{
            .pan = glm::vec2(14.0f, -7.0f),
            .zoom = 1.75f,
            .min_zoom = 0.25f,
            .max_zoom = 4.0f,
        }
    );
  });

  const auto transform = runtime.view_transform(canvas_widget);
  ASSERT_TRUE(transform.has_value());
  EXPECT_EQ(transform->pan, glm::vec2(14.0f, -7.0f));
  EXPECT_FLOAT_EQ(transform->zoom, 1.75f);

  const auto *node = document->node(runtime.node_id_for(canvas_widget));
  ASSERT_NE(node, nullptr);
  EXPECT_TRUE(node->view_transform_interaction.enabled);
  EXPECT_TRUE(node->view_transform_interaction.middle_mouse_pan);
  EXPECT_TRUE(node->view_transform_interaction.wheel_zoom);
}

TEST(UIFoundationsTest, ImmediateRuntimeAppliesGraphViewTransformRequests) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.graph_view("graph", make_runtime_graph_spec()).widget_id();
    ui.set_view_transform(
        widget,
        UIViewTransform2D{
            .pan = glm::vec2(-18.0f, 9.0f),
            .zoom = 2.25f,
            .min_zoom = 0.25f,
            .max_zoom = 4.0f,
        }
    );
  });

  const auto transform = runtime.view_transform(widget);
  ASSERT_TRUE(transform.has_value());
  EXPECT_EQ(transform->pan, glm::vec2(-18.0f, 9.0f));
  EXPECT_FLOAT_EQ(transform->zoom, 2.25f);

  const auto *node = document->node(runtime.node_id_for(widget));
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->type, NodeType::GraphView);
  EXPECT_TRUE(node->view_transform_interaction.enabled);
  EXPECT_TRUE(node->view_transform_interaction.middle_mouse_pan);
  EXPECT_TRUE(node->view_transform_interaction.wheel_zoom);
}

TEST(UIFoundationsTest, ImmediateRuntimeGraphViewPreservesIdentityAndTransientStateAcrossRerender) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId widget = k_invalid_widget_id;
  runtime.render([&](Frame &ui) {
    widget = ui.graph_view("graph", make_runtime_graph_spec()).widget_id();
  });

  const UINodeId node_id = runtime.node_id_for(widget);
  ASSERT_NE(node_id, k_invalid_node_id);

  document->set_view_transform(
      node_id,
      UIViewTransform2D{
          .pan = glm::vec2(22.0f, -11.0f),
          .zoom = 1.6f,
          .min_zoom = 0.25f,
          .max_zoom = 4.0f,
      }
  );

  auto *node = document->node(node_id);
  ASSERT_NE(node, nullptr);
  node->graph_view.hovered_target = UIGraphSemanticTarget{
      .semantic = UIGraphHitSemantic::NodeHeader,
      .primary_id = k_runtime_graph_node_a,
  };
  node->graph_view.drag.mode = UIGraphDragMode::Marquee;
  node->graph_view.marquee_visible = true;
  node->graph_view.marquee_start_world = glm::vec2(4.0f, 8.0f);
  node->graph_view.marquee_current_world = glm::vec2(24.0f, 32.0f);

  runtime.render([&](Frame &ui) {
    widget =
        ui.graph_view("graph", make_runtime_graph_spec(glm::vec2(96.0f, 112.0f)))
            .widget_id();
  });

  EXPECT_EQ(runtime.node_id_for(widget), node_id);

  const auto transform = runtime.view_transform(widget);
  ASSERT_TRUE(transform.has_value());
  EXPECT_EQ(transform->pan, glm::vec2(22.0f, -11.0f));
  EXPECT_FLOAT_EQ(transform->zoom, 1.6f);

  const auto selection = runtime.graph_selection(widget);
  ASSERT_TRUE(selection.has_value());
  EXPECT_TRUE(selection->node_ids.empty());

  node = document->node(node_id);
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->graph_view.model.nodes.size(), 2u);
  EXPECT_EQ(node->graph_view.model.nodes[0].position, glm::vec2(96.0f, 112.0f));
  ASSERT_TRUE(node->graph_view.hovered_target.has_value());
  EXPECT_EQ(node->graph_view.hovered_target->semantic, UIGraphHitSemantic::NodeHeader);
  EXPECT_EQ(node->graph_view.hovered_target->primary_id, k_runtime_graph_node_a);
  EXPECT_EQ(node->graph_view.drag.mode, UIGraphDragMode::Marquee);
  EXPECT_TRUE(node->graph_view.marquee_visible);
  EXPECT_EQ(node->graph_view.marquee_start_world, glm::vec2(4.0f, 8.0f));
  EXPECT_EQ(node->graph_view.marquee_current_world, glm::vec2(24.0f, 32.0f));
}

TEST(UIFoundationsTest, ImmediateRuntimeForwardsViewTransformChangeCallback) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId canvas_widget = k_invalid_widget_id;
  bool received_change = false;
  runtime.render([&](Frame &ui) {
    auto canvas = ui.view("canvas")
                      .view_transform_enabled()
                      .on_view_transform_change(
                          [&](const UIViewTransformChangeEvent &event) {
                            received_change =
                                event.current.zoom == 2.0f &&
                                event.anchor_world == glm::vec2(3.0f, 5.0f);
                          }
                      );
    canvas_widget = canvas.widget_id();
  });

  const auto *node = document->node(runtime.node_id_for(canvas_widget));
  ASSERT_NE(node, nullptr);
  ASSERT_TRUE(node->on_view_transform_change);
  node->on_view_transform_change(UIViewTransformChangeEvent{
      .current =
          UIViewTransform2D{
              .zoom = 2.0f,
          },
      .anchor_world = glm::vec2(3.0f, 5.0f),
  });

  EXPECT_TRUE(received_change);
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

TEST(UIFoundationsTest, ImmediateRuntimeAnonymousContainersPreserveDescendantIdentity) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId outer_widget = k_invalid_widget_id;
  WidgetId inner_widget = k_invalid_widget_id;
  WidgetId field_widget = k_invalid_widget_id;

  runtime.render([&](Frame &ui) {
    auto outer = ui.column();
    outer_widget = outer.widget_id();
    auto inner = outer.row();
    inner_widget = inner.widget_id();
    field_widget = inner.text_input("field", "first").widget_id();
  });

  const UINodeId outer_node = runtime.node_id_for(outer_widget);
  const UINodeId inner_node = runtime.node_id_for(inner_widget);
  const UINodeId field_node = runtime.node_id_for(field_widget);
  ASSERT_NE(outer_node, k_invalid_node_id);
  ASSERT_NE(inner_node, k_invalid_node_id);
  ASSERT_NE(field_node, k_invalid_node_id);

  runtime.render([&](Frame &ui) {
    auto outer = ui.column();
    outer_widget = outer.widget_id();
    auto inner = outer.row();
    inner_widget = inner.widget_id();
    field_widget = inner.text_input("field", "second").widget_id();
  });

  EXPECT_EQ(runtime.node_id_for(outer_widget), outer_node);
  EXPECT_EQ(runtime.node_id_for(inner_widget), inner_node);
  EXPECT_EQ(runtime.node_id_for(field_widget), field_node);
  EXPECT_EQ(document->node(field_node)->text, "second");
}

TEST(UIFoundationsTest, ImmediateRuntimeAnonymousContainersIgnoreNamedSiblingInsertions) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId host = document->create_view();
  document->append_child(root, host);
  document->set_root(root);

  Runtime runtime(document, host);

  WidgetId shell_widget = k_invalid_widget_id;
  WidgetId field_widget = k_invalid_widget_id;

  runtime.render([&](Frame &ui) {
    ui.view("header");
    auto shell = ui.column();
    shell_widget = shell.widget_id();
    field_widget = shell.text_input("field", "first").widget_id();
  });

  const UINodeId shell_node = runtime.node_id_for(shell_widget);
  const UINodeId field_node = runtime.node_id_for(field_widget);
  ASSERT_NE(shell_node, k_invalid_node_id);
  ASSERT_NE(field_node, k_invalid_node_id);

  runtime.render([&](Frame &ui) {
    ui.view("header");
    ui.view("toolbar");
    auto shell = ui.column();
    shell_widget = shell.widget_id();
    field_widget = shell.text_input("field", "second").widget_id();
  });

  EXPECT_EQ(runtime.node_id_for(shell_widget), shell_node);
  EXPECT_EQ(runtime.node_id_for(field_widget), field_node);
  EXPECT_EQ(document->node(field_node)->text, "second");
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
