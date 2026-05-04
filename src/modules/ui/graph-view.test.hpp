#include "canvas/view-transform.hpp"
#include "document/document.hpp"
#include "dsl.hpp"
#include "layout/layout.hpp"
#include "widgets/graph-layout.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <utility>
#include <vector>

namespace astralix::ui {
namespace {

constexpr UIGraphId k_node_a = 1001u;
constexpr UIGraphId k_node_b = 1002u;
constexpr UIGraphId k_port_a_in = 2001u;
constexpr UIGraphId k_port_a_out = 2002u;
constexpr UIGraphId k_port_b_in = 2003u;
constexpr UIGraphId k_port_b_out = 2004u;
constexpr UIGraphId k_edge_ab = 3001u;

UIGraphViewModel sample_graph_model() {
  return UIGraphViewModel{
      .nodes =
          {
              UIGraphNode{
                  .id = k_node_a,
                  .title = "Oscillator",
                  .position = glm::vec2(80.0f, 60.0f),
                  .input_ports = {k_port_a_in},
                  .output_ports = {k_port_a_out},
              },
              UIGraphNode{
                  .id = k_node_b,
                  .title = "Filter",
                  .position = glm::vec2(360.0f, 180.0f),
                  .input_ports = {k_port_b_in},
                  .output_ports = {k_port_b_out},
              },
          },
      .ports =
          {
              UIGraphPort{
                  .id = k_port_a_in,
                  .node_id = k_node_a,
                  .direction = UIGraphPortDirection::Input,
                  .label = "FM",
                  .color = glm::vec4(0.87f, 0.61f, 0.36f, 1.0f),
              },
              UIGraphPort{
                  .id = k_port_a_out,
                  .node_id = k_node_a,
                  .direction = UIGraphPortDirection::Output,
                  .label = "Out",
                  .color = glm::vec4(0.36f, 0.73f, 0.94f, 1.0f),
              },
              UIGraphPort{
                  .id = k_port_b_in,
                  .node_id = k_node_b,
                  .direction = UIGraphPortDirection::Input,
                  .label = "In",
                  .color = glm::vec4(0.62f, 0.84f, 0.47f, 1.0f),
              },
              UIGraphPort{
                  .id = k_port_b_out,
                  .node_id = k_node_b,
                  .direction = UIGraphPortDirection::Output,
                  .label = "Out",
                  .color = glm::vec4(0.94f, 0.52f, 0.64f, 1.0f),
              },
          },
      .edges =
          {
              UIGraphEdge{
                  .id = k_edge_ab,
                  .from_port_id = k_port_a_out,
                  .to_port_id = k_port_b_in,
                  .color = glm::vec4(0.62f, 0.82f, 1.0f, 1.0f),
                  .thickness = 2.0f,
              },
          },
  };
}

UILayoutContext sample_graph_context() {
  return UILayoutContext{
      .viewport_size = glm::vec2(900.0f, 640.0f),
      .default_font_size = 16.0f,
  };
}

std::pair<Ref<UIDocument>, UINodeId> make_graph_document() {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId graph = document->create_graph_view();

  document->set_root(root);
  document->append_child(root, graph);
  document->mutate_style(root, [](UIStyle &style) {
    style.width = UILength::pixels(900.0f);
    style.height = UILength::pixels(640.0f);
    style.padding = UIEdges::all(20.0f);
  });
  document->mutate_style(graph, [](UIStyle &style) {
    style.width = UILength::pixels(860.0f);
    style.height = UILength::pixels(600.0f);
  });
  document->set_graph_view_model(graph, sample_graph_model());
  layout_document(*document, sample_graph_context());
  return {document, graph};
}

const UIGraphNodeLayoutInfo *find_node_layout(
    const UIDocument::UINode &graph_node,
    UIGraphId node_id
) {
  for (const auto &item : graph_node.layout.graph_view.node_layouts) {
    if (item.id == node_id) {
      return &item;
    }
  }
  return nullptr;
}

const UIGraphPortLayoutInfo *find_port_layout(
    const UIDocument::UINode &graph_node,
    UIGraphId port_id
) {
  for (const auto &item : graph_node.layout.graph_view.port_layouts) {
    if (item.id == port_id) {
      return &item;
    }
  }
  return nullptr;
}

const UIGraphEdgeLayoutInfo *find_edge_layout(
    const UIDocument::UINode &graph_node,
    UIGraphId edge_id
) {
  for (const auto &item : graph_node.layout.graph_view.edge_layouts) {
    if (item.id == edge_id) {
      return &item;
    }
  }
  return nullptr;
}

UIGraphNode *find_model_node(UIGraphViewModel &model, UIGraphId node_id) {
  for (auto &node : model.nodes) {
    if (node.id == node_id) {
      return &node;
    }
  }
  return nullptr;
}

glm::vec2 graph_screen_from_world(
    const UIDocument::UINode &graph_node,
    glm::vec2 world_position
) {
  return canvas_to_screen(
      graph_node.view_transform.value_or(UIViewTransform2D{}),
      world_position,
      glm::vec2(
          graph_node.layout.content_bounds.x,
          graph_node.layout.content_bounds.y
      )
  );
}

glm::vec2 graph_local_from_screen(
    const UIDocument::UINode &graph_node,
    glm::vec2 screen_position
) {
  return glm::vec2(
      screen_position.x - graph_node.layout.content_bounds.x,
      screen_position.y - graph_node.layout.content_bounds.y
  );
}

glm::vec2 cubic_point(
    glm::vec2 p0,
    glm::vec2 p1,
    glm::vec2 p2,
    glm::vec2 p3,
    float t
) {
  const float u = 1.0f - t;
  return u * u * u * p0 + 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 +
         t * t * t * p3;
}

UIPointerEvent make_pointer_event(
    const UIDocument::UINode &graph_node,
    UIPointerEventPhase phase,
    glm::vec2 screen_position,
    std::optional<UICustomHitData> custom,
    std::optional<input::MouseButton> button = input::MouseButton::Left,
    input::KeyModifiers modifiers = {},
    glm::vec2 total_delta = glm::vec2(0.0f)
) {
  const bool left_down = button.has_value() &&
                         *button == input::MouseButton::Left &&
                         phase != UIPointerEventPhase::Release;
  return UIPointerEvent{
      .phase = phase,
      .screen_position = screen_position,
      .local_position = graph_local_from_screen(graph_node, screen_position),
      .delta = glm::vec2(0.0f),
      .total_delta = total_delta,
      .button = button,
      .buttons =
          UIPointerButtons{
              .left = left_down,
          },
      .modifiers = modifiers,
      .part = UIHitPart::Body,
      .item_index = std::nullopt,
      .custom = std::move(custom),
  };
}

TEST(UIFoundationsTest, GraphViewDocumentDefaultsAndModelAreInstalled) {
  auto document = UIDocument::create();
  const UINodeId graph = document->create_graph_view();
  document->set_graph_view_model(graph, sample_graph_model());

  const auto *node = document->node(graph);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->type, NodeType::GraphView);
  EXPECT_EQ(node->style.overflow, Overflow::Hidden);
  EXPECT_TRUE(node->view_transform_interaction.enabled);
  EXPECT_TRUE(node->view_transform_interaction.middle_mouse_pan);
  EXPECT_TRUE(node->view_transform_interaction.wheel_zoom);
  EXPECT_TRUE(node->view_transform.has_value());
  EXPECT_TRUE(static_cast<bool>(node->on_pointer_event));
  EXPECT_TRUE(static_cast<bool>(node->on_custom_hit_test));
  ASSERT_EQ(node->graph_view.model.nodes.size(), 2u);
  ASSERT_EQ(node->graph_view.model.ports.size(), 4u);
  ASSERT_EQ(node->graph_view.model.edges.size(), 1u);
}

TEST(UIFoundationsTest, GraphDagLayoutPlacesChainAcrossLayers) {
  constexpr UIGraphId k_layout_node_a = 4101u;
  constexpr UIGraphId k_layout_node_b = 4102u;
  constexpr UIGraphId k_layout_node_c = 4103u;
  constexpr UIGraphId k_layout_port_a_out = 4201u;
  constexpr UIGraphId k_layout_port_b_in = 4202u;
  constexpr UIGraphId k_layout_port_b_out = 4203u;
  constexpr UIGraphId k_layout_port_c_in = 4204u;

  UIGraphViewModel model{
      .nodes =
          {
              UIGraphNode{
                  .id = k_layout_node_a,
                  .title = "A",
                  .output_ports = {k_layout_port_a_out},
              },
              UIGraphNode{
                  .id = k_layout_node_b,
                  .title = "B",
                  .input_ports = {k_layout_port_b_in},
                  .output_ports = {k_layout_port_b_out},
              },
              UIGraphNode{
                  .id = k_layout_node_c,
                  .title = "C",
                  .input_ports = {k_layout_port_c_in},
              },
          },
      .ports =
          {
              UIGraphPort{
                  .id = k_layout_port_a_out,
                  .node_id = k_layout_node_a,
                  .direction = UIGraphPortDirection::Output,
              },
              UIGraphPort{
                  .id = k_layout_port_b_in,
                  .node_id = k_layout_node_b,
                  .direction = UIGraphPortDirection::Input,
              },
              UIGraphPort{
                  .id = k_layout_port_b_out,
                  .node_id = k_layout_node_b,
                  .direction = UIGraphPortDirection::Output,
              },
              UIGraphPort{
                  .id = k_layout_port_c_in,
                  .node_id = k_layout_node_c,
                  .direction = UIGraphPortDirection::Input,
              },
          },
      .edges =
          {
              UIGraphEdge{
                  .id = 4301u,
                  .from_port_id = k_layout_port_a_out,
                  .to_port_id = k_layout_port_b_in,
              },
              UIGraphEdge{
                  .id = 4302u,
                  .from_port_id = k_layout_port_b_out,
                  .to_port_id = k_layout_port_c_in,
              },
          },
  };

  layout_graph_dag(model);

  const auto *node_a = find_model_node(model, k_layout_node_a);
  const auto *node_b = find_model_node(model, k_layout_node_b);
  const auto *node_c = find_model_node(model, k_layout_node_c);
  ASSERT_NE(node_a, nullptr);
  ASSERT_NE(node_b, nullptr);
  ASSERT_NE(node_c, nullptr);
  EXPECT_LT(node_a->position.x, node_b->position.x);
  EXPECT_LT(node_b->position.x, node_c->position.x);
}

TEST(UIFoundationsTest, GraphDagLayoutPreservesFixedNodePositions) {
  constexpr UIGraphId k_layout_node_left = 4401u;
  constexpr UIGraphId k_layout_node_middle = 4402u;
  constexpr UIGraphId k_layout_node_right = 4403u;
  constexpr UIGraphId k_layout_port_left_out = 4501u;
  constexpr UIGraphId k_layout_port_middle_in = 4502u;
  constexpr UIGraphId k_layout_port_middle_out = 4503u;
  constexpr UIGraphId k_layout_port_right_in = 4504u;

  UIGraphViewModel model{
      .nodes =
          {
              UIGraphNode{
                  .id = k_layout_node_left,
                  .title = "Left",
                  .output_ports = {k_layout_port_left_out},
              },
              UIGraphNode{
                  .id = k_layout_node_middle,
                  .title = "Middle",
                  .position = glm::vec2(420.0f, 210.0f),
                  .input_ports = {k_layout_port_middle_in},
                  .output_ports = {k_layout_port_middle_out},
              },
              UIGraphNode{
                  .id = k_layout_node_right,
                  .title = "Right",
                  .input_ports = {k_layout_port_right_in},
              },
          },
      .ports =
          {
              UIGraphPort{
                  .id = k_layout_port_left_out,
                  .node_id = k_layout_node_left,
                  .direction = UIGraphPortDirection::Output,
              },
              UIGraphPort{
                  .id = k_layout_port_middle_in,
                  .node_id = k_layout_node_middle,
                  .direction = UIGraphPortDirection::Input,
              },
              UIGraphPort{
                  .id = k_layout_port_middle_out,
                  .node_id = k_layout_node_middle,
                  .direction = UIGraphPortDirection::Output,
              },
              UIGraphPort{
                  .id = k_layout_port_right_in,
                  .node_id = k_layout_node_right,
                  .direction = UIGraphPortDirection::Input,
              },
          },
      .edges =
          {
              UIGraphEdge{
                  .id = 4601u,
                  .from_port_id = k_layout_port_left_out,
                  .to_port_id = k_layout_port_middle_in,
              },
              UIGraphEdge{
                  .id = 4602u,
                  .from_port_id = k_layout_port_middle_out,
                  .to_port_id = k_layout_port_right_in,
              },
          },
  };

  layout_graph_dag(
      model,
      std::unordered_set<UIGraphId>{k_layout_node_middle}
  );

  const auto *node_left = find_model_node(model, k_layout_node_left);
  const auto *node_middle = find_model_node(model, k_layout_node_middle);
  const auto *node_right = find_model_node(model, k_layout_node_right);
  ASSERT_NE(node_left, nullptr);
  ASSERT_NE(node_middle, nullptr);
  ASSERT_NE(node_right, nullptr);
  EXPECT_EQ(node_middle->position, glm::vec2(420.0f, 210.0f));
  EXPECT_LT(node_left->position.x, node_middle->position.x);
  EXPECT_GT(node_right->position.x, node_middle->position.x);
}

TEST(UIFoundationsTest, GraphViewDslMaterializesGraphNodeAndCallbacks) {
  using namespace dsl;

  auto document = UIDocument::create();
  UINodeId graph = k_invalid_node_id;
  bool selection_changed = false;
  bool transform_changed = false;

  mount(
      *document,
      graph_view(GraphViewSpec{
          .model = sample_graph_model(),
      })
          .bind(graph)
          .on_selection_change(
              [&](const UIGraphSelection &) { selection_changed = true; }
          )
          .on_view_transform_change(
              [&](const UIViewTransformChangeEvent &) {
                transform_changed = true;
              }
          )
  );

  const auto *node = document->node(graph);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->type, NodeType::GraphView);
  ASSERT_EQ(node->graph_view.model.nodes.size(), 2u);
  ASSERT_EQ(node->graph_view.model.ports.size(), 4u);
  ASSERT_EQ(node->graph_view.model.edges.size(), 1u);
  ASSERT_TRUE(static_cast<bool>(node->graph_view.on_selection_change));
  ASSERT_TRUE(static_cast<bool>(node->on_view_transform_change));

  node->graph_view.on_selection_change(UIGraphSelection{});
  node->on_view_transform_change(UIViewTransformChangeEvent{});
  EXPECT_TRUE(selection_changed);
  EXPECT_TRUE(transform_changed);
}

TEST(UIFoundationsTest, GraphViewHitTestingResolvesSemanticTargets) {
  auto [document, graph] = make_graph_document();
  const auto *graph_node = document->node(graph);
  ASSERT_NE(graph_node, nullptr);

  const auto *node_layout = find_node_layout(*graph_node, k_node_a);
  const auto *port_layout = find_port_layout(*graph_node, k_port_a_out);
  const auto *edge_layout = find_edge_layout(*graph_node, k_edge_ab);
  ASSERT_NE(node_layout, nullptr);
  ASSERT_NE(port_layout, nullptr);
  ASSERT_NE(edge_layout, nullptr);

  const glm::vec2 header_point = graph_screen_from_world(
      *graph_node,
      glm::vec2(
          node_layout->header_world_bounds.x +
              node_layout->header_world_bounds.width * 0.5f,
          node_layout->header_world_bounds.y +
              node_layout->header_world_bounds.height * 0.5f
      )
  );
  auto header_hit = hit_test_document(*document, header_point);
  ASSERT_TRUE(header_hit.has_value());
  ASSERT_TRUE(header_hit->custom.has_value());
  EXPECT_EQ(header_hit->custom->semantic,
            static_cast<uint32_t>(UIGraphHitSemantic::NodeHeader));
  EXPECT_EQ(header_hit->custom->primary_id, k_node_a);

  const glm::vec2 port_point =
      graph_screen_from_world(*graph_node, port_layout->socket_world_center);
  auto port_hit = hit_test_document(*document, port_point);
  ASSERT_TRUE(port_hit.has_value());
  ASSERT_TRUE(port_hit->custom.has_value());
  EXPECT_EQ(port_hit->custom->semantic,
            static_cast<uint32_t>(UIGraphHitSemantic::Port));
  EXPECT_EQ(port_hit->custom->primary_id, k_port_a_out);
  EXPECT_EQ(port_hit->custom->secondary_id, k_node_a);

  const glm::vec2 edge_point = graph_screen_from_world(
      *graph_node,
      cubic_point(
          edge_layout->start_world,
          edge_layout->control_a_world,
          edge_layout->control_b_world,
          edge_layout->end_world,
          0.5f
      )
  );
  auto edge_hit = hit_test_document(*document, edge_point);
  ASSERT_TRUE(edge_hit.has_value());
  ASSERT_TRUE(edge_hit->custom.has_value());
  EXPECT_EQ(edge_hit->custom->semantic,
            static_cast<uint32_t>(UIGraphHitSemantic::Edge));
  EXPECT_EQ(edge_hit->custom->primary_id, k_edge_ab);

  const glm::vec2 background_point(
      graph_node->layout.content_bounds.x + 24.0f,
      graph_node->layout.content_bounds.y + 24.0f
  );
  auto background_hit = hit_test_document(*document, background_point);
  ASSERT_TRUE(background_hit.has_value());
  ASSERT_TRUE(background_hit->custom.has_value());
  EXPECT_EQ(background_hit->custom->semantic,
            static_cast<uint32_t>(UIGraphHitSemantic::Background));
}

TEST(UIFoundationsTest, GraphViewDrawListContainsNodesPortsAndEdges) {
  auto [document, graph] = make_graph_document();
  build_draw_list(*document, sample_graph_context());

  bool saw_oscillator_title = false;
  bool saw_filter_title = false;
  size_t path_commands = 0u;
  size_t rect_commands = 0u;

  for (const auto &command : document->draw_list().commands) {
    if (command.node_id != graph) {
      continue;
    }

    if (command.type == DrawCommandType::Path) {
      ++path_commands;
    } else if (command.type == DrawCommandType::Rect) {
      ++rect_commands;
    } else if (command.type == DrawCommandType::Text &&
               command.text == "Oscillator") {
      saw_oscillator_title = true;
    } else if (command.type == DrawCommandType::Text &&
               command.text == "Filter") {
      saw_filter_title = true;
    }
  }

  EXPECT_GE(path_commands, 3u);
  EXPECT_GE(rect_commands, 4u);
  EXPECT_TRUE(saw_oscillator_title);
  EXPECT_TRUE(saw_filter_title);
}

TEST(UIFoundationsTest, GraphViewSelectionRulesUseStableIds) {
  auto [document, graph] = make_graph_document();
  auto *graph_node = document->node(graph);
  ASSERT_NE(graph_node, nullptr);
  ASSERT_TRUE(graph_node->on_pointer_event);

  std::optional<UIGraphSelection> last_selection;
  document->set_on_graph_selection_change(
      graph,
      [&](const UIGraphSelection &selection) { last_selection = selection; }
  );

  const auto *node_layout = find_node_layout(*graph_node, k_node_a);
  const auto *edge_layout = find_edge_layout(*graph_node, k_edge_ab);
  ASSERT_NE(node_layout, nullptr);
  ASSERT_NE(edge_layout, nullptr);

  const glm::vec2 header_point = graph_screen_from_world(
      *graph_node,
      glm::vec2(
          node_layout->header_world_bounds.x + 32.0f,
          node_layout->header_world_bounds.y + 12.0f
      )
  );
  auto header_hit = hit_test_document(*document, header_point);
  ASSERT_TRUE(header_hit.has_value());
  ASSERT_TRUE(header_hit->custom.has_value());
  graph_node->on_pointer_event(
      make_pointer_event(
          *graph_node,
          UIPointerEventPhase::Press,
          header_point,
          header_hit->custom
      )
  );

  ASSERT_TRUE(last_selection.has_value());
  EXPECT_EQ(last_selection->node_ids, std::vector<UIGraphId>{k_node_a});
  EXPECT_TRUE(last_selection->edge_ids.empty());

  const glm::vec2 edge_point = graph_screen_from_world(
      *graph_node,
      cubic_point(
          edge_layout->start_world,
          edge_layout->control_a_world,
          edge_layout->control_b_world,
          edge_layout->end_world,
          0.5f
      )
  );
  auto edge_hit = hit_test_document(*document, edge_point);
  ASSERT_TRUE(edge_hit.has_value());
  ASSERT_TRUE(edge_hit->custom.has_value());
  graph_node->on_pointer_event(
      make_pointer_event(
          *graph_node,
          UIPointerEventPhase::Press,
          edge_point,
          edge_hit->custom,
          input::MouseButton::Left,
          input::KeyModifiers{.shift = true}
      )
  );

  ASSERT_TRUE(last_selection.has_value());
  EXPECT_EQ(last_selection->node_ids, std::vector<UIGraphId>{k_node_a});
  EXPECT_EQ(last_selection->edge_ids, std::vector<UIGraphId>{k_edge_ab});

  const glm::vec2 background_point(
      graph_node->layout.content_bounds.x + 24.0f,
      graph_node->layout.content_bounds.y + 24.0f
  );
  auto background_hit = hit_test_document(*document, background_point);
  ASSERT_TRUE(background_hit.has_value());
  ASSERT_TRUE(background_hit->custom.has_value());

  graph_node->on_pointer_event(
      make_pointer_event(
          *graph_node,
          UIPointerEventPhase::Press,
          background_point,
          background_hit->custom
      )
  );
  EXPECT_EQ(last_selection->node_ids, std::vector<UIGraphId>{k_node_a});
  EXPECT_EQ(last_selection->edge_ids, std::vector<UIGraphId>{k_edge_ab});

  graph_node->on_pointer_event(
      make_pointer_event(
          *graph_node,
          UIPointerEventPhase::Release,
          background_point,
          background_hit->custom
      )
  );

  ASSERT_TRUE(last_selection.has_value());
  EXPECT_TRUE(last_selection->node_ids.empty());
  EXPECT_TRUE(last_selection->edge_ids.empty());
}

TEST(UIFoundationsTest, GraphViewBackgroundDragBuildsMarqueeSelection) {
  auto [document, graph] = make_graph_document();
  auto *graph_node = document->node(graph);
  ASSERT_NE(graph_node, nullptr);
  ASSERT_TRUE(graph_node->on_pointer_event);

  std::optional<UIGraphSelection> last_selection;
  document->set_on_graph_selection_change(
      graph,
      [&](const UIGraphSelection &selection) { last_selection = selection; }
  );

  const auto *node_layout = find_node_layout(*graph_node, k_node_a);
  ASSERT_NE(node_layout, nullptr);

  const glm::vec2 start_point(
      graph_node->layout.content_bounds.x + 18.0f,
      graph_node->layout.content_bounds.y + 18.0f
  );
  const glm::vec2 end_point = graph_screen_from_world(
      *graph_node,
      glm::vec2(
          node_layout->world_bounds.right() + 12.0f,
          node_layout->world_bounds.bottom() + 12.0f
      )
  );
  const glm::vec2 drag_delta = end_point - start_point;

  auto start_hit = hit_test_document(*document, start_point);
  ASSERT_TRUE(start_hit.has_value());
  ASSERT_TRUE(start_hit->custom.has_value());

  graph_node->on_pointer_event(
      make_pointer_event(
          *graph_node,
          UIPointerEventPhase::Press,
          start_point,
          start_hit->custom
      )
  );
  graph_node->on_pointer_event(
      make_pointer_event(
          *graph_node,
          UIPointerEventPhase::DragStart,
          start_point,
          start_hit->custom,
          input::MouseButton::Left,
          {},
          drag_delta
      )
  );
  graph_node->on_pointer_event(
      make_pointer_event(
          *graph_node,
          UIPointerEventPhase::DragUpdate,
          end_point,
          start_hit->custom,
          input::MouseButton::Left,
          {},
          drag_delta
      )
  );
  graph_node->on_pointer_event(
      make_pointer_event(
          *graph_node,
          UIPointerEventPhase::DragEnd,
          end_point,
          start_hit->custom,
          input::MouseButton::Left,
          {},
          drag_delta
      )
  );
  graph_node->on_pointer_event(
      make_pointer_event(
          *graph_node,
          UIPointerEventPhase::Release,
          end_point,
          start_hit->custom
      )
  );

  ASSERT_TRUE(last_selection.has_value());
  EXPECT_EQ(last_selection->node_ids, std::vector<UIGraphId>{k_node_a});
  EXPECT_TRUE(last_selection->edge_ids.empty());
}

TEST(UIFoundationsTest, GraphViewNodeDragAndConnectionDragCallbacksUseStableIds) {
  {
    auto [document, graph] = make_graph_document();
    auto *graph_node = document->node(graph);
    ASSERT_NE(graph_node, nullptr);
    ASSERT_TRUE(graph_node->on_pointer_event);

    std::optional<std::pair<UIGraphId, glm::vec2>> move_event;
    document->set_on_graph_node_move(
        graph,
        [&](UIGraphId node_id, glm::vec2 position) {
          move_event = std::make_pair(node_id, position);
        }
    );

    const auto *node_layout = find_node_layout(*graph_node, k_node_a);
    ASSERT_NE(node_layout, nullptr);
    const glm::vec2 press_point = graph_screen_from_world(
        *graph_node,
        glm::vec2(
            node_layout->header_world_bounds.x + 48.0f,
            node_layout->header_world_bounds.y + 14.0f
        )
    );
    const glm::vec2 drag_delta(48.0f, 32.0f);
    const glm::vec2 drag_point = press_point + drag_delta;
    auto press_hit = hit_test_document(*document, press_point);
    ASSERT_TRUE(press_hit.has_value());
    ASSERT_TRUE(press_hit->custom.has_value());

    graph_node->on_pointer_event(
        make_pointer_event(
            *graph_node,
            UIPointerEventPhase::Press,
            press_point,
            press_hit->custom
        )
    );
    graph_node->on_pointer_event(
        make_pointer_event(
            *graph_node,
            UIPointerEventPhase::DragStart,
            press_point,
            press_hit->custom,
            input::MouseButton::Left,
            {},
            drag_delta
        )
    );
    graph_node->on_pointer_event(
        make_pointer_event(
            *graph_node,
            UIPointerEventPhase::DragUpdate,
            drag_point,
            press_hit->custom,
            input::MouseButton::Left,
            {},
            drag_delta
        )
    );

    ASSERT_TRUE(move_event.has_value());
    EXPECT_EQ(move_event->first, k_node_a);
    EXPECT_EQ(move_event->second, glm::vec2(128.0f, 92.0f));

    const auto *updated = document->node(graph);
    ASSERT_NE(updated, nullptr);
    ASSERT_FALSE(updated->graph_view.model.nodes.empty());
    EXPECT_EQ(updated->graph_view.model.nodes[0].position, glm::vec2(128.0f, 92.0f));

    graph_node->on_pointer_event(
        make_pointer_event(
            *graph_node,
            UIPointerEventPhase::DragEnd,
            drag_point,
            press_hit->custom,
            input::MouseButton::Left,
            {},
            drag_delta
        )
    );
    graph_node->on_pointer_event(
        make_pointer_event(
            *graph_node,
            UIPointerEventPhase::Release,
            drag_point,
            press_hit->custom
        )
    );
  }

  {
    auto [document, graph] = make_graph_document();
    auto *graph_node = document->node(graph);
    ASSERT_NE(graph_node, nullptr);
    ASSERT_TRUE(graph_node->on_pointer_event);

    std::optional<std::pair<UIGraphId, std::optional<UIGraphId>>> connection_event;
    document->set_on_graph_connection_drag_end(
        graph,
        [&](UIGraphId from_port_id, std::optional<UIGraphId> to_port_id) {
          connection_event = std::make_pair(from_port_id, to_port_id);
        }
    );

    const auto *from_port = find_port_layout(*graph_node, k_port_a_out);
    const auto *to_port = find_port_layout(*graph_node, k_port_b_in);
    ASSERT_NE(from_port, nullptr);
    ASSERT_NE(to_port, nullptr);

    const glm::vec2 from_point =
        graph_screen_from_world(*graph_node, from_port->socket_world_center);
    const glm::vec2 to_point =
        graph_screen_from_world(*graph_node, to_port->socket_world_center);
    auto from_hit = hit_test_document(*document, from_point);
    auto to_hit = hit_test_document(*document, to_point);
    ASSERT_TRUE(from_hit.has_value());
    ASSERT_TRUE(from_hit->custom.has_value());
    ASSERT_TRUE(to_hit.has_value());
    ASSERT_TRUE(to_hit->custom.has_value());

    graph_node->on_pointer_event(
        make_pointer_event(
            *graph_node,
            UIPointerEventPhase::Press,
            from_point,
            from_hit->custom
        )
    );
    graph_node->on_pointer_event(
        make_pointer_event(
            *graph_node,
            UIPointerEventPhase::DragStart,
            from_point,
            from_hit->custom
        )
    );
    graph_node->on_pointer_event(
        make_pointer_event(
            *graph_node,
            UIPointerEventPhase::DragUpdate,
            to_point,
            to_hit->custom,
            input::MouseButton::Left,
            {},
            to_point - from_point
        )
    );
    graph_node->on_pointer_event(
        make_pointer_event(
            *graph_node,
            UIPointerEventPhase::DragEnd,
            to_point,
            to_hit->custom,
            input::MouseButton::Left,
            {},
            to_point - from_point
        )
    );
    graph_node->on_pointer_event(
        make_pointer_event(
            *graph_node,
            UIPointerEventPhase::Release,
            to_point,
            to_hit->custom
        )
    );

    ASSERT_TRUE(connection_event.has_value());
    EXPECT_EQ(connection_event->first, k_port_a_out);
    ASSERT_TRUE(connection_event->second.has_value());
    EXPECT_EQ(*connection_event->second, k_port_b_in);

    const auto *updated = document->node(graph);
    ASSERT_NE(updated, nullptr);
    EXPECT_FALSE(updated->graph_view.connection_preview.has_value());
  }
}

TEST(UIFoundationsTest, GraphViewHitTestingRespectsViewTransform) {
  auto [document, graph] = make_graph_document();
  document->set_view_transform(
      graph,
      UIViewTransform2D{
          .pan = glm::vec2(-70.0f, -36.0f),
          .zoom = 1.75f,
          .min_zoom = 0.25f,
          .max_zoom = 4.0f,
      }
  );
  layout_document(*document, sample_graph_context());

  const auto *graph_node = document->node(graph);
  ASSERT_NE(graph_node, nullptr);
  const auto *port_layout = find_port_layout(*graph_node, k_port_b_in);
  ASSERT_NE(port_layout, nullptr);

  const glm::vec2 point =
      graph_screen_from_world(*graph_node, port_layout->socket_world_center);
  auto hit = hit_test_document(*document, point);
  ASSERT_TRUE(hit.has_value());
  ASSERT_TRUE(hit->custom.has_value());
  EXPECT_EQ(hit->custom->semantic,
            static_cast<uint32_t>(UIGraphHitSemantic::Port));
  EXPECT_EQ(hit->custom->primary_id, k_port_b_in);
}

} // namespace
} // namespace astralix::ui
