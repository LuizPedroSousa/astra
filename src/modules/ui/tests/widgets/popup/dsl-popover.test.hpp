#include "document/document.hpp"
#include "dsl.hpp"
#include "layout/layout.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, DeclarativeDslSupportsPopoverMenuCompositesAndSecondaryClicks) {
  using namespace dsl;
  using namespace dsl::styles;

  auto document = UIDocument::create();
  UINodeId popup = k_invalid_node_id;
  UINodeId item = k_invalid_node_id;
  UINodeId separator = k_invalid_node_id;
  UINodeId submenu = k_invalid_node_id;
  UINodeId context_target = k_invalid_node_id;

  mount(
      *document,
      dsl::column().style(fill()).children(
          view()
              .bind(context_target)
              .on_secondary_click(
                  [](const UIPointerButtonEvent &) {}
              ),
          popover()
              .bind(popup)
              .style(width(px(180.0f)))
              .children(
                  menu_item("Open", [] {}).bind(item),
                  menu_separator().bind(separator),
                  submenu_item("More", [] {}).bind(submenu)
              )
      )
  );

  const auto *popup_node = document->node(popup);
  const auto *item_node = document->node(item);
  const auto *separator_node = document->node(separator);
  const auto *submenu_node = document->node(submenu);
  const auto *context_target_node = document->node(context_target);
  ASSERT_NE(popup_node, nullptr);
  ASSERT_NE(item_node, nullptr);
  ASSERT_NE(separator_node, nullptr);
  ASSERT_NE(submenu_node, nullptr);
  ASSERT_NE(context_target_node, nullptr);

  EXPECT_EQ(popup_node->type, NodeType::Popover);
  EXPECT_FALSE(popup_node->visible);
  ASSERT_EQ(popup_node->children.size(), 3u);
  EXPECT_EQ(item_node->type, NodeType::Pressable);
  EXPECT_EQ(separator_node->type, NodeType::View);
  EXPECT_EQ(submenu_node->type, NodeType::Pressable);
  EXPECT_FALSE(static_cast<bool>(submenu_node->on_hover));
  EXPECT_TRUE(static_cast<bool>(submenu_node->on_click));
  EXPECT_TRUE(static_cast<bool>(context_target_node->on_secondary_click));
}

TEST(UIFoundationsTest, FixedWidthPopoversDoNotExpandToViewportWidth) {
  using namespace dsl;
  using namespace dsl::styles;

  auto document = UIDocument::create();
  UINodeId popup = k_invalid_node_id;

  mount(
      *document,
      dsl::column().style(fill()).children(
          popover()
              .bind(popup)
              .style(width(px(220.0f)), padding(8.0f))
              .children(menu_item("Open", [] {}))
      )
  );

  document->open_popover_at(
      popup,
      glm::vec2(24.0f, 32.0f),
      UIPopupPlacement::BottomStart
  );

  layout_document(
      *document,
      UILayoutContext{
          .viewport_size = glm::vec2(1280.0f, 720.0f),
          .default_font_size = 16.0f,
      }
  );

  const auto *popup_node = document->node(popup);
  ASSERT_NE(popup_node, nullptr);
  EXPECT_FLOAT_EQ(popup_node->layout.bounds.width, 220.0f);
  EXPECT_FLOAT_EQ(popup_node->layout.popover.popup_rect.width, 220.0f);
}

} // namespace
} // namespace astralix::ui
