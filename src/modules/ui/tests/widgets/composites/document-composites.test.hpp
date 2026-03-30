#include "document/document.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, ButtonAndIconButtonExposeExpectedDefaults) {
  auto document = UIDocument::create();
  const UINodeId button =
      document->create_button("Spawn", [] {}, "spawn_button");
  const UINodeId icon_button =
      document->create_icon_button("icons::clear", [] {}, "icon_button");

  const auto *button_node = document->node(button);
  const auto *icon_button_node = document->node(icon_button);
  ASSERT_NE(button_node, nullptr);
  ASSERT_NE(icon_button_node, nullptr);

  EXPECT_TRUE(button_node->focusable);
  ASSERT_TRUE(button_node->style.cursor.has_value());
  EXPECT_EQ(*button_node->style.cursor, CursorStyle::Pointer);
  EXPECT_TRUE(button_node->style.hovered_style.background_color.has_value());
  EXPECT_TRUE(button_node->style.pressed_style.background_color.has_value());
  EXPECT_TRUE(button_node->style.focused_style.border_color.has_value());
  EXPECT_TRUE(button_node->style.disabled_style.opacity.has_value());

  EXPECT_EQ(icon_button_node->type, NodeType::Pressable);
  EXPECT_TRUE(icon_button_node->focusable);
  ASSERT_TRUE(icon_button_node->style.cursor.has_value());
  EXPECT_EQ(*icon_button_node->style.cursor, CursorStyle::Pointer);
  ASSERT_EQ(icon_button_node->children.size(), 1u);
  const auto *icon_node = document->node(icon_button_node->children.front());
  ASSERT_NE(icon_node, nullptr);
  EXPECT_EQ(icon_node->type, NodeType::Image);
  EXPECT_EQ(icon_node->texture_id, "icons::clear");

  document->set_focused_node(button);
  document->set_focusable(button, false);
  EXPECT_EQ(document->focused_node(), k_invalid_node_id);
}

} // namespace
} // namespace astralix::ui
