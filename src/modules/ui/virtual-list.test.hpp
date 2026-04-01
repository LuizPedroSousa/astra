#include "virtual-list.hpp"

#include <gtest/gtest.h>
#include <utility>
#include <vector>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, VirtualListControllerPoolsSlotsAndTracksVisibleRange) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId scroll_view = document->create_scroll_view();
  document->set_root(root);
  document->append_child(root, scroll_view);
  document->mutate_style(scroll_view, [](UIStyle &style) { style.gap = 4.0f; });

  auto *scroll_node = document->node(scroll_view);
  ASSERT_NE(scroll_node, nullptr);
  scroll_node->layout.scroll.viewport_size = glm::vec2(120.0f, 48.0f);
  scroll_node->layout.scroll.offset = glm::vec2(0.0f, 18.0f);

  std::vector<UINodeId> created_slots;
  std::vector<std::pair<size_t, size_t>> bound_items;
  VirtualListController controller(
      document, scroll_view, [&](size_t slot_index) {
                    const UINodeId slot = document->create_view();
                    created_slots.push_back(slot);
                    return slot;
                  }, [&](size_t slot_index, UINodeId, size_t item_index) {
                    bound_items.emplace_back(slot_index, item_index);
                  }
  );
  controller.set_overscan(1u);
  controller.set_item_count(5u);
  controller.set_item_height(0u, 10.0f);
  controller.set_item_height(1u, 20.0f);
  controller.set_item_height(2u, 30.0f);
  controller.set_item_height(3u, 40.0f);
  controller.set_item_height(4u, 50.0f);
  controller.set_content_width(180.0f);

  controller.refresh(true);
  EXPECT_TRUE(controller.current_range().valid);
  EXPECT_EQ(controller.current_range().start, 0u);
  EXPECT_EQ(controller.current_range().end, 3u);
  EXPECT_EQ(controller.pool_size(), 4u);
  ASSERT_EQ(bound_items.size(), 4u);
  const std::pair<size_t, size_t> initial_first_binding{0u, 0u};
  const std::pair<size_t, size_t> initial_last_binding{3u, 3u};
  EXPECT_EQ(bound_items[0], initial_first_binding);
  EXPECT_EQ(bound_items[3], initial_last_binding);

  const auto *first_slot = document->node(created_slots[0]);
  const auto *bottom_spacer = document->node(controller.bottom_spacer());
  ASSERT_NE(first_slot, nullptr);
  ASSERT_NE(bottom_spacer, nullptr);
  EXPECT_FLOAT_EQ(first_slot->style.width.value, 180.0f);
  EXPECT_FLOAT_EQ(first_slot->style.height.value, 10.0f);
  EXPECT_TRUE(bottom_spacer->visible);
  EXPECT_FLOAT_EQ(bottom_spacer->style.height.value, 50.0f);

  bound_items.clear();
  scroll_node = document->node(scroll_view);
  ASSERT_NE(scroll_node, nullptr);
  scroll_node->layout.scroll.offset = glm::vec2(0.0f, 76.0f);
  controller.refresh();

  EXPECT_TRUE(controller.current_range().valid);
  EXPECT_EQ(controller.current_range().start, 2u);
  EXPECT_EQ(controller.current_range().end, 4u);
  EXPECT_EQ(controller.pool_size(), 4u);
  ASSERT_EQ(bound_items.size(), 3u);
  const std::pair<size_t, size_t> shifted_first_binding{0u, 2u};
  const std::pair<size_t, size_t> shifted_last_binding{2u, 4u};
  EXPECT_EQ(bound_items[0], shifted_first_binding);
  EXPECT_EQ(bound_items[2], shifted_last_binding);

  const auto *top_spacer = document->node(controller.top_spacer());
  ASSERT_NE(top_spacer, nullptr);
  EXPECT_TRUE(top_spacer->visible);
  EXPECT_FLOAT_EQ(top_spacer->style.height.value, 34.0f);
  EXPECT_FLOAT_EQ(document->node(created_slots[0])->style.height.value, 30.0f);
}

} // namespace
} // namespace astralix::ui
