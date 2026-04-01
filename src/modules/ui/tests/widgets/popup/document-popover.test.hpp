#include "document/document.hpp"
#include "layout/layout.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, PopoverNodesExposeExpectedDefaultsAndOpenAtCursor) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId popover = document->create_popover();

  document->set_root(root);
  document->append_child(root, popover);

  const auto *popover_node = document->node(popover);
  ASSERT_NE(popover_node, nullptr);
  EXPECT_EQ(popover_node->type, NodeType::Popover);
  EXPECT_FALSE(popover_node->visible);
  EXPECT_TRUE(popover_node->enabled);
  EXPECT_FALSE(popover_node->focusable);
  EXPECT_EQ(popover_node->style.position_type, PositionType::Absolute);

  document->open_popover_at(
      popover,
      glm::vec2(48.0f, 72.0f),
      UIPopupPlacement::BottomStart,
      1u
  );

  popover_node = document->node(popover);
  ASSERT_NE(popover_node, nullptr);
  EXPECT_TRUE(popover_node->visible);
  EXPECT_TRUE(popover_node->popover.open);
  EXPECT_EQ(popover_node->popover.anchor_kind, UIPopupAnchorKind::Cursor);
  EXPECT_FLOAT_EQ(popover_node->popover.anchor_point.x, 48.0f);
  EXPECT_FLOAT_EQ(popover_node->popover.anchor_point.y, 72.0f);
  EXPECT_EQ(popover_node->popover.placement, UIPopupPlacement::BottomStart);
  EXPECT_EQ(popover_node->popover.depth, 1u);
  ASSERT_EQ(document->open_popover_stack().size(), 1u);
  EXPECT_EQ(document->open_popover_stack().front(), popover);
}

TEST(UIFoundationsTest, PopoversCanAnchorToNodesAndCollapseFromDepth) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId anchor = document->create_view();
  const UINodeId root_popover = document->create_popover();
  const UINodeId submenu = document->create_popover();
  const UINodeId deep_submenu = document->create_popover();

  document->set_root(root);
  document->append_child(root, anchor);
  document->append_child(root, root_popover);
  document->append_child(root, submenu);
  document->append_child(root, deep_submenu);

  document->open_popover_anchored_to(
      root_popover,
      anchor,
      UIPopupPlacement::BottomStart,
      0u
  );
  document->open_popover_anchored_to(
      submenu,
      anchor,
      UIPopupPlacement::RightStart,
      1u
  );
  document->open_popover_anchored_to(
      deep_submenu,
      anchor,
      UIPopupPlacement::RightStart,
      2u
  );

  const auto *submenu_node = document->node(submenu);
  ASSERT_NE(submenu_node, nullptr);
  EXPECT_EQ(submenu_node->popover.anchor_kind, UIPopupAnchorKind::Node);
  EXPECT_EQ(submenu_node->popover.anchor_node_id, anchor);
  EXPECT_EQ(submenu_node->popover.placement, UIPopupPlacement::RightStart);

  document->close_popovers_from_depth(1u);

  const auto *root_popover_node = document->node(root_popover);
  const auto *deep_submenu_node = document->node(deep_submenu);
  ASSERT_NE(root_popover_node, nullptr);
  ASSERT_NE(deep_submenu_node, nullptr);
  EXPECT_TRUE(root_popover_node->popover.open);
  EXPECT_FALSE(submenu_node->popover.open);
  EXPECT_FALSE(deep_submenu_node->popover.open);
  ASSERT_EQ(document->open_popover_stack().size(), 1u);
  EXPECT_EQ(document->open_popover_stack().front(), root_popover);
}

TEST(UIFoundationsTest, OpeningPopoversAndLegacyInputsCloseEachOther) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId select = document->create_select({"Alpha", "Beta"}, 0u, {});
  const UINodeId popover = document->create_popover();

  document->set_root(root);
  document->append_child(root, select);
  document->append_child(root, popover);

  document->set_select_open(select, true);
  EXPECT_TRUE(document->select_open(select));
  EXPECT_EQ(document->open_popup_node(), select);

  document->open_popover_at(
      popover,
      glm::vec2(32.0f, 32.0f),
      UIPopupPlacement::BottomStart
  );
  EXPECT_FALSE(document->select_open(select));
  EXPECT_EQ(document->open_popup_node(), k_invalid_node_id);
  ASSERT_EQ(document->open_popover_stack().size(), 1u);

  document->set_select_open(select, true);
  EXPECT_TRUE(document->select_open(select));
  EXPECT_TRUE(document->open_popover_stack().empty());
  const auto *popover_node = document->node(popover);
  ASSERT_NE(popover_node, nullptr);
  EXPECT_FALSE(popover_node->popover.open);
  EXPECT_FALSE(popover_node->visible);
}

TEST(UIFoundationsTest, HitTestingPrefersTopmostPopoverOverlay) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId root_popover = document->create_popover();
  const UINodeId submenu = document->create_popover();

  document->set_root(root);
  document->append_child(root, root_popover);
  document->append_child(root, submenu);

  document->mutate_style(root_popover, [](UIStyle &style) {
    style.width = UILength::pixels(120.0f);
    style.height = UILength::pixels(48.0f);
    style.background_color = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
  });
  document->mutate_style(submenu, [](UIStyle &style) {
    style.width = UILength::pixels(120.0f);
    style.height = UILength::pixels(48.0f);
    style.background_color = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);
  });

  document->open_popover_at(
      root_popover,
      glm::vec2(40.0f, 40.0f),
      UIPopupPlacement::BottomStart,
      0u
  );
  document->open_popover_at(
      submenu,
      glm::vec2(40.0f, 40.0f),
      UIPopupPlacement::BottomStart,
      1u
  );

  layout_document(
      *document,
      UILayoutContext{
          .viewport_size = glm::vec2(400.0f, 300.0f),
          .default_font_size = 16.0f,
      }
  );

  auto hit = hit_test_document(*document, glm::vec2(50.0f, 50.0f));
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(hit->node_id, submenu);
}

} // namespace
} // namespace astralix::ui
