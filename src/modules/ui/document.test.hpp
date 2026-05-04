#include "document/document.hpp"
#include "foundations.hpp"
#include "layout/layout.hpp"
#include "systems/ui-system/core.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, CollectsFocusableOrderAndNearestAncestors) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId button = document->create_pressable();
  const UINodeId button_label = document->create_text("press");
  const UINodeId custom_focusable = document->create_view();
  const UINodeId hidden_button = document->create_pressable();
  const UINodeId disabled_button = document->create_pressable();
  const UINodeId scroll_container = document->create_view();
  const UINodeId leaf = document->create_text("leaf");

  document->set_root(root);
  document->append_child(root, button);
  document->append_child(button, button_label);
  document->append_child(root, custom_focusable);
  document->append_child(root, hidden_button);
  document->append_child(root, disabled_button);
  document->append_child(custom_focusable, scroll_container);
  document->append_child(scroll_container, leaf);

  document->set_focusable(custom_focusable, true);
  document->set_visible(hidden_button, false);
  document->set_enabled(disabled_button, false);
  document->mutate_style(scroll_container, [](UIStyle &style) {
    style.scroll_mode = ScrollMode::Vertical;
  });

  const std::vector<UINodeId> order = collect_focusable_order(*document);
  ASSERT_EQ(order.size(), 2u);
  EXPECT_EQ(order[0], button);
  EXPECT_EQ(order[1], custom_focusable);

  EXPECT_EQ(find_nearest_focusable_ancestor(*document, button_label), button);
  EXPECT_EQ(find_nearest_pressable_ancestor(*document, button_label), button);
  EXPECT_EQ(find_nearest_scrollable_ancestor(*document, leaf), scroll_container);
  EXPECT_EQ(find_nearest_focusable_ancestor(*document, leaf), custom_focusable);
  EXPECT_FALSE(find_nearest_focusable_ancestor(*document, root).has_value());
}

TEST(UIFoundationsTest, DocumentStateMutationsUpdateFocusCaretAndScrollState) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId node = document->create_text_input("stateful", "placeholder");

  document->set_root(root);
  document->append_child(root, node);
  document->clear_dirty();

  document->set_hot_node(node);
  document->set_active_node(node);
  document->set_focused_node(node);
  document->set_text_selection(
      node, UITextSelection{.anchor = 4u, .focus = 1u}
  );
  document->set_caret(node, 3u, true);
  document->set_scroll_offset(node, glm::vec2(12.0f, 24.0f));

  const auto *ui_node = document->node(node);
  ASSERT_NE(ui_node, nullptr);
  EXPECT_TRUE(ui_node->paint_state.hovered);
  EXPECT_TRUE(ui_node->paint_state.pressed);
  EXPECT_TRUE(ui_node->paint_state.focused);
  EXPECT_EQ(ui_node->selection.start(), 1u);
  EXPECT_EQ(ui_node->selection.end(), 4u);
  EXPECT_TRUE(ui_node->caret.active);
  EXPECT_TRUE(ui_node->caret.visible);
  EXPECT_EQ(ui_node->caret.index, 3u);
  ASSERT_NE(document->scroll_state(node), nullptr);
  EXPECT_EQ(document->scroll_state(node)->offset, glm::vec2(12.0f, 24.0f));
  EXPECT_TRUE(document->layout_dirty());

  document->set_enabled(node, false);
  EXPECT_EQ(document->hot_node(), k_invalid_node_id);
  EXPECT_EQ(document->active_node(), k_invalid_node_id);
  EXPECT_EQ(document->focused_node(), k_invalid_node_id);
  EXPECT_FALSE(ui_node->paint_state.hovered);
  EXPECT_FALSE(ui_node->paint_state.pressed);
  EXPECT_FALSE(ui_node->paint_state.focused);
}

TEST(UIFoundationsTest, DocumentCanQueueAndConsumeFocusRequests) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId input = document->create_text_input({}, "Placeholder");

  document->set_root(root);
  document->append_child(root, input);

  document->request_focus(input);
  EXPECT_EQ(document->requested_focus(), input);
  EXPECT_EQ(document->consume_requested_focus(), input);
  EXPECT_EQ(document->requested_focus(), k_invalid_node_id);
}

TEST(UIFoundationsTest, DocumentTracksPointerCallbacksAndCaptureRequests) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId node = document->create_view();

  document->set_root(root);
  document->append_child(root, node);

  bool received_pointer_event = false;
  document->set_on_pointer_event(node, [&](const UIPointerEvent &event) {
    received_pointer_event = event.phase == UIPointerEventPhase::Press &&
                             event.button == input::MouseButton::Middle;
  });

  const auto *ui_node = document->node(node);
  ASSERT_NE(ui_node, nullptr);
  ASSERT_TRUE(ui_node->on_pointer_event);

  ui_node->on_pointer_event(UIPointerEvent{
      .phase = UIPointerEventPhase::Press,
      .button = input::MouseButton::Middle,
  });
  EXPECT_TRUE(received_pointer_event);

  document->request_pointer_capture(node, input::MouseButton::Middle);
  document->release_pointer_capture(node, input::MouseButton::Middle);

  const auto requests = document->consume_pointer_capture_requests();
  ASSERT_EQ(requests.size(), 2u);
  EXPECT_EQ(requests[0].node_id, node);
  EXPECT_EQ(requests[0].button, input::MouseButton::Middle);
  EXPECT_EQ(requests[0].action, UIPointerCaptureAction::Capture);
  EXPECT_EQ(requests[1].action, UIPointerCaptureAction::Release);
  EXPECT_TRUE(document->consume_pointer_capture_requests().empty());
}

TEST(UIFoundationsTest, DocumentTracksCustomHitTestCallbacks) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId node = document->create_view();

  document->set_root(root);
  document->append_child(root, node);

  bool received_local_position = false;
  document->set_on_custom_hit_test(
      node,
      [&](glm::vec2 local_position) -> std::optional<UICustomHitData> {
        received_local_position = local_position == glm::vec2(4.0f, 9.0f);
        return UICustomHitData{
            .semantic = 7u,
            .primary_id = 11u,
            .secondary_id = 13u,
        };
      }
  );

  const auto *ui_node = document->node(node);
  ASSERT_NE(ui_node, nullptr);
  ASSERT_TRUE(ui_node->on_custom_hit_test);

  auto hit = ui_node->on_custom_hit_test(glm::vec2(4.0f, 9.0f));
  ASSERT_TRUE(hit.has_value());
  EXPECT_TRUE(received_local_position);
  EXPECT_EQ(hit->semantic, 7u);
  EXPECT_EQ(hit->primary_id, 11u);
  EXPECT_EQ(hit->secondary_id, 13u);
}

TEST(UIFoundationsTest, DocumentHitTestingReturnsCustomSemanticPayload) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId surface = document->create_view();

  document->set_root(root);
  document->append_child(root, surface);

  document->mutate_style(root, [](UIStyle &style) {
    style.width = UILength::pixels(200.0f);
    style.height = UILength::pixels(160.0f);
  });
  document->mutate_style(surface, [](UIStyle &style) {
    style.width = UILength::pixels(120.0f);
    style.height = UILength::pixels(80.0f);
    style.padding = UIEdges::all(10.0f);
  });

  std::optional<glm::vec2> received_local_position;
  document->set_on_custom_hit_test(
      surface,
      [&](glm::vec2 local_position) -> std::optional<UICustomHitData> {
        received_local_position = local_position;
        return UICustomHitData{
            .semantic = 5u,
            .primary_id = 42u,
            .secondary_id = 77u,
        };
      }
  );

  layout_document(
      *document,
      UILayoutContext{
          .viewport_size = glm::vec2(200.0f, 160.0f),
          .default_font_size = 16.0f,
      }
  );

  const auto *surface_node = document->node(surface);
  ASSERT_NE(surface_node, nullptr);
  const glm::vec2 point(
      surface_node->layout.content_bounds.x + 15.0f,
      surface_node->layout.content_bounds.y + 20.0f
  );

  auto hit = hit_test_document(*document, point);
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(hit->node_id, surface);
  EXPECT_EQ(hit->part, UIHitPart::Body);
  EXPECT_FALSE(hit->item_index.has_value());
  ASSERT_TRUE(received_local_position.has_value());
  EXPECT_EQ(*received_local_position, glm::vec2(15.0f, 20.0f));
  ASSERT_TRUE(hit->custom.has_value());
  EXPECT_EQ(hit->custom->semantic, 5u);
  EXPECT_EQ(hit->custom->primary_id, 42u);
  EXPECT_EQ(hit->custom->secondary_id, 77u);
  EXPECT_EQ(hit->custom->local_position, glm::vec2(15.0f, 20.0f));
}

TEST(UIFoundationsTest, BuiltInHitTestingKeepsUsingItemIndexWithoutCustomPayload) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId segmented =
      document->create_segmented_control({"All", "Info", "Warn"}, 0u);

  document->set_root(root);
  document->append_child(root, segmented);

  document->mutate_style(root, [](UIStyle &style) {
    style.width = UILength::pixels(320.0f);
    style.height = UILength::pixels(96.0f);
  });
  document->mutate_style(segmented, [](UIStyle &style) {
    style.width = UILength::pixels(240.0f);
    style.height = UILength::pixels(36.0f);
  });

  layout_document(
      *document,
      UILayoutContext{
          .viewport_size = glm::vec2(320.0f, 96.0f),
          .default_font_size = 16.0f,
      }
  );

  const auto *segmented_node = document->node(segmented);
  ASSERT_NE(segmented_node, nullptr);
  ASSERT_GE(segmented_node->layout.segmented_control.item_rects.size(), 2u);
  const UIRect item_rect =
      segmented_node->layout.segmented_control.item_rects[1];
  const glm::vec2 point(
      item_rect.x + item_rect.width * 0.5f,
      item_rect.y + item_rect.height * 0.5f
  );

  auto hit = hit_test_document(*document, point);
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(hit->node_id, segmented);
  EXPECT_EQ(hit->part, UIHitPart::SegmentedControlItem);
  ASSERT_TRUE(hit->item_index.has_value());
  EXPECT_EQ(*hit->item_index, 1u);
  EXPECT_FALSE(hit->custom.has_value());
}

TEST(UIFoundationsTest, CustomInteractiveNodesAreHoverableAndPreserveSemanticHits) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId handler = document->create_view();
  const UINodeId child = document->create_view();

  document->set_root(root);
  document->append_child(root, handler);
  document->append_child(handler, child);

  document->set_on_pointer_event(handler, [](const UIPointerEvent &) {});
  document->set_on_custom_hit_test(
      child,
      [](glm::vec2) -> std::optional<UICustomHitData> {
        return UICustomHitData{
            .semantic = 9u,
            .primary_id = 101u,
            .secondary_id = 202u,
            .local_position = glm::vec2(3.0f, 6.0f),
        };
      }
  );

  const ui_system_core::RootEntry entry{
      .entity_id = EntityID{1u},
      .root = nullptr,
      .document = document,
  };
  auto hover_target = ui_system_core::find_hoverable_target(entry, child);
  ASSERT_TRUE(hover_target.has_value());
  EXPECT_EQ(hover_target->node_id, child);

  auto routed_hit = ui_system_core::find_pointer_event_target(
      entry,
      child,
      UIHitPart::Body,
      std::nullopt,
      UICustomHitData{
          .semantic = 9u,
          .primary_id = 101u,
          .secondary_id = 202u,
          .local_position = glm::vec2(3.0f, 6.0f),
      }
  );
  ASSERT_TRUE(routed_hit.has_value());
  EXPECT_EQ(routed_hit->target.node_id, handler);
  ASSERT_TRUE(routed_hit->custom.has_value());
  EXPECT_EQ(routed_hit->custom->semantic, 9u);
  EXPECT_EQ(routed_hit->custom->primary_id, 101u);
  EXPECT_EQ(routed_hit->custom->secondary_id, 202u);
  EXPECT_EQ(routed_hit->custom->local_position, glm::vec2(3.0f, 6.0f));
}

TEST(UIFoundationsTest, DocumentTracksViewTransformStateAndCallbacks) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId node = document->create_view();

  document->set_root(root);
  document->append_child(root, node);

  bool received_change = false;
  document->set_on_view_transform_change(
      node,
      [&](const UIViewTransformChangeEvent &event) {
        received_change = event.current.zoom == 1.5f &&
                          event.anchor_screen == glm::vec2(8.0f, 12.0f);
      }
  );
  document->set_view_transform_enabled(node, true);
  document->set_view_transform_middle_mouse_pan(node, true);
  document->set_view_transform_wheel_zoom(node, true);
  document->set_view_transform(
      node,
      UIViewTransform2D{
          .pan = glm::vec2(10.0f, -4.0f),
          .zoom = 1.5f,
          .min_zoom = 0.25f,
          .max_zoom = 4.0f,
      }
  );

  const auto transform = document->view_transform(node);
  ASSERT_TRUE(transform.has_value());
  EXPECT_EQ(transform->pan, glm::vec2(10.0f, -4.0f));
  EXPECT_FLOAT_EQ(transform->zoom, 1.5f);

  const auto *ui_node = document->node(node);
  ASSERT_NE(ui_node, nullptr);
  EXPECT_TRUE(ui_node->view_transform_interaction.enabled);
  EXPECT_TRUE(ui_node->view_transform_interaction.middle_mouse_pan);
  EXPECT_TRUE(ui_node->view_transform_interaction.wheel_zoom);
  ASSERT_TRUE(ui_node->on_view_transform_change);

  ui_node->on_view_transform_change(UIViewTransformChangeEvent{
      .current =
          UIViewTransform2D{
              .zoom = 1.5f,
          },
      .anchor_screen = glm::vec2(8.0f, 12.0f),
  });
  EXPECT_TRUE(received_change);
}

TEST(UIFoundationsTest, DocumentCanSuppressNextCharacterInput) {
  auto document = UIDocument::create();

  document->suppress_next_character_input(static_cast<uint32_t>('`'));

  EXPECT_FALSE(
      document->consume_suppressed_character_input(static_cast<uint32_t>('a'))
  );
  EXPECT_TRUE(
      document->consume_suppressed_character_input(static_cast<uint32_t>('`'))
  );
  EXPECT_FALSE(
      document->consume_suppressed_character_input(static_cast<uint32_t>('`'))
  );
}

TEST(UIFoundationsTest, CallbackQueueCanBeMutatedDuringFlush) {
  auto document = UIDocument::create();
  int calls = 0;

  document->queue_callback([&]() {
    ++calls;
    document->queue_callback([&]() { ++calls; });
  });

  document->flush_callbacks();
  EXPECT_EQ(calls, 1);

  document->flush_callbacks();
  EXPECT_EQ(calls, 2);
}

} // namespace
} // namespace astralix::ui
