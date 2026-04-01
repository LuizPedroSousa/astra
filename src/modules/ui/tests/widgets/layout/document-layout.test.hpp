#include "document/document.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, ScrollViewAndSplitterExposeExpectedDefaults) {
  auto document = UIDocument::create();
  const UINodeId scroll_view = document->create_scroll_view();
  const UINodeId splitter = document->create_splitter();

  const auto *scroll_view_node = document->node(scroll_view);
  ASSERT_NE(scroll_view_node, nullptr);
  EXPECT_EQ(scroll_view_node->type, NodeType::ScrollView);
  EXPECT_FALSE(scroll_view_node->focusable);
  EXPECT_EQ(scroll_view_node->style.overflow, Overflow::Hidden);
  EXPECT_EQ(scroll_view_node->style.scroll_mode, ScrollMode::Vertical);
  EXPECT_EQ(
      scroll_view_node->style.scrollbar_visibility,
      ScrollbarVisibility::Auto
  );

  const auto *splitter_node = document->node(splitter);
  ASSERT_NE(splitter_node, nullptr);
  EXPECT_EQ(splitter_node->type, NodeType::Splitter);
  EXPECT_FLOAT_EQ(splitter_node->style.flex_shrink, 0.0f);
  EXPECT_EQ(splitter_node->style.align_self, AlignSelf::Stretch);
}

} // namespace
} // namespace astralix::ui
