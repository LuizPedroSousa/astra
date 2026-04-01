#include "document/document.hpp"
#include "foundations.hpp"

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
