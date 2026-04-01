#include "document/document.hpp"
#include "dsl.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, DeclarativeDslAppliesButtonRulesOnTopOfDefaults) {
  using namespace dsl;
  using namespace dsl::styles;

  auto document = UIDocument::create();
  UINodeId button_id = k_invalid_node_id;

  mount(
      *document,
      dsl::column().children(
          button("Spawn", [] {})
              .bind(button_id)
              .style(
                  padding_xy(20.0f, 6.0f),
                  hover(state().background(rgba(0.2f, 0.3f, 0.5f, 1.0f))),
                  focused(state().border(3.0f, rgba(0.8f, 0.9f, 1.0f, 1.0f))),
                  disabled(state().opacity(0.25f))
              )
      )
  );

  const auto *button_node = document->node(button_id);
  ASSERT_NE(button_node, nullptr);
  EXPECT_TRUE(button_node->focusable);
  EXPECT_FLOAT_EQ(button_node->style.padding.left, 20.0f);
  EXPECT_FLOAT_EQ(button_node->style.padding.top, 6.0f);
  ASSERT_TRUE(button_node->style.cursor.has_value());
  EXPECT_EQ(*button_node->style.cursor, CursorStyle::Pointer);
  ASSERT_TRUE(button_node->style.hovered_style.background_color.has_value());
  EXPECT_EQ(
      *button_node->style.hovered_style.background_color,
      rgba(0.2f, 0.3f, 0.5f, 1.0f)
  );
  ASSERT_TRUE(button_node->style.focused_style.border_width.has_value());
  EXPECT_FLOAT_EQ(*button_node->style.focused_style.border_width, 3.0f);
  ASSERT_TRUE(button_node->style.disabled_style.opacity.has_value());
  EXPECT_FLOAT_EQ(*button_node->style.disabled_style.opacity, 0.25f);
}

} // namespace
} // namespace astralix::ui
