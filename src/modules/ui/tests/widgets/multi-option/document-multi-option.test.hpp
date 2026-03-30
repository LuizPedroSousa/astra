#include "document/document.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, SegmentedControlAndChipGroupExposeDefaults) {
  auto document = UIDocument::create();
  const UINodeId segmented = document->create_segmented_control(
      {"All", "Info", "Warn"}, 5u, "segmented"
  );
  const UINodeId chips = document->create_chip_group(
      {"Logs", "Commands", "Output"}, {}, "chips"
  );

  const auto *segmented_node = document->node(segmented);
  const auto *chip_node = document->node(chips);
  ASSERT_NE(segmented_node, nullptr);
  ASSERT_NE(chip_node, nullptr);

  EXPECT_EQ(segmented_node->type, NodeType::SegmentedControl);
  EXPECT_TRUE(segmented_node->focusable);
  ASSERT_TRUE(segmented_node->style.cursor.has_value());
  EXPECT_EQ(*segmented_node->style.cursor, CursorStyle::Pointer);
  ASSERT_EQ(segmented_node->segmented_control.options.size(), 3u);
  EXPECT_EQ(segmented_node->segmented_control.selected_index, 2u);

  EXPECT_EQ(chip_node->type, NodeType::ChipGroup);
  ASSERT_TRUE(chip_node->style.cursor.has_value());
  EXPECT_EQ(*chip_node->style.cursor, CursorStyle::Pointer);
  ASSERT_EQ(chip_node->chip_group.options.size(), 3u);
  ASSERT_EQ(chip_node->chip_group.selected.size(), 3u);
  EXPECT_TRUE(chip_node->chip_group.selected[0]);
  EXPECT_TRUE(chip_node->chip_group.selected[1]);
  EXPECT_TRUE(chip_node->chip_group.selected[2]);
}

TEST(UIFoundationsTest, SegmentedControlAndChipGroupDocumentStateCanBeUpdated) {
  auto document = UIDocument::create();
  const UINodeId segmented = document->create_segmented_control(
      {"All", "Info", "Warn"}, 1u, "segmented"
  );
  const UINodeId chips = document->create_chip_group(
      {"Logs", "Commands", "Output"}, {true, false, true}, "chips"
  );

  document->set_segmented_selected_index(segmented, 2u);
  document->set_chip_selected(chips, 1u, true);
  document->set_chip_selected(chips, 2u, false);

  EXPECT_EQ(document->segmented_selected_index(segmented), 2u);
  EXPECT_TRUE(document->chip_selected(chips, 0u));
  EXPECT_TRUE(document->chip_selected(chips, 1u));
  EXPECT_FALSE(document->chip_selected(chips, 2u));
}

} // namespace
} // namespace astralix::ui
