#include "document/document.hpp"
#include "dsl.hpp"
#include "foundations.hpp"
#include "layout/layout.hpp"

#include <gtest/gtest.h>
#include <limits>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, ResizableViewsAndSplittersExposeExpectedHelpers) {
  auto document = UIDocument::create();
  const UINodeId panel = document->create_view("panel");
  const UINodeId header = document->create_view("header");
  const UINodeId splitter = document->create_splitter("splitter");
  document->mutate_style(panel, [](UIStyle &style) {
    style.position_type = PositionType::Absolute;
    style.draggable = true;
    style.resize_mode = ResizeMode::Both;
    style.resize_edges = k_resize_edge_all;
    style.resize_handle_thickness = 8.0f;
    style.resize_corner_extent = 18.0f;
  });
  document->mutate_style(header, [](UIStyle &style) { style.drag_handle = true; });

  const auto *panel_node = document->node(panel);
  const auto *header_node = document->node(header);
  const auto *splitter_node = document->node(splitter);
  ASSERT_NE(panel_node, nullptr);
  ASSERT_NE(header_node, nullptr);
  ASSERT_NE(splitter_node, nullptr);
  EXPECT_TRUE(node_supports_panel_drag(*panel_node));
  EXPECT_TRUE(node_supports_panel_resize(*panel_node));
  EXPECT_TRUE(node_is_drag_handle(*header_node));
  EXPECT_FALSE(node_supports_panel_resize(*splitter_node));
  EXPECT_TRUE(resize_allows_horizontal(panel_node->style.resize_mode));
  EXPECT_TRUE(resize_allows_vertical(panel_node->style.resize_mode));
  EXPECT_TRUE(
      has_resize_edge(panel_node->style.resize_edges, k_resize_edge_left)
  );
  EXPECT_TRUE(
      has_resize_edge(panel_node->style.resize_edges, k_resize_edge_bottom)
  );
}

TEST(UIFoundationsTest, ClampsResizablePanelBoundsToParentContent) {
  const UIRect parent_bounds{
      .x = 32.0f, .y = 24.0f, .width = 640.0f, .height = 360.0f
  };
  const UIRect start_bounds{
      .x = 160.0f, .y = 96.0f, .width = 240.0f, .height = 180.0f
  };

  const UIRect top_drag = clamp_panel_resize_bounds(
      start_bounds,
      UIRect{
          .x = start_bounds.x,
          .y = -48.0f,
          .width = start_bounds.width,
          .height = 324.0f,
      },
      parent_bounds,
      UIHitPart::ResizeTop,
      120.0f,
      std::numeric_limits<float>::infinity(),
      80.0f,
      std::numeric_limits<float>::infinity()
  );
  EXPECT_FLOAT_EQ(top_drag.y, parent_bounds.y);
  EXPECT_FLOAT_EQ(top_drag.height, start_bounds.bottom() - parent_bounds.y);

  const UIRect bottom_right_drag = clamp_panel_resize_bounds(
      start_bounds,
      UIRect{
          .x = start_bounds.x,
          .y = start_bounds.y,
          .width = 640.0f,
          .height = 420.0f,
      },
      parent_bounds,
      UIHitPart::ResizeBottomRight,
      120.0f,
      std::numeric_limits<float>::infinity(),
      80.0f,
      std::numeric_limits<float>::infinity()
  );
  EXPECT_FLOAT_EQ(bottom_right_drag.x, start_bounds.x);
  EXPECT_FLOAT_EQ(bottom_right_drag.y, start_bounds.y);
  EXPECT_FLOAT_EQ(bottom_right_drag.width, parent_bounds.right() - start_bounds.x);
  EXPECT_FLOAT_EQ(bottom_right_drag.height, parent_bounds.bottom() - start_bounds.y);

  const UIRect left_drag = clamp_panel_resize_bounds(
      start_bounds,
      UIRect{
          .x = 0.0f,
          .y = start_bounds.y,
          .width = 400.0f,
          .height = start_bounds.height,
      },
      parent_bounds,
      UIHitPart::ResizeLeft,
      120.0f,
      std::numeric_limits<float>::infinity(),
      80.0f,
      std::numeric_limits<float>::infinity()
  );
  EXPECT_FLOAT_EQ(left_drag.x, parent_bounds.x);
  EXPECT_FLOAT_EQ(left_drag.width, start_bounds.right() - parent_bounds.x);
}

TEST(UIFoundationsTest, ClampsResolvedAbsolutePanelRectsAfterViewportShrink) {
  const UIRect viewport{.x = 0.0f, .y = 0.0f, .width = 400.0f, .height = 300.0f};

  const UIRect shifted = clamp_rect_to_bounds(
      UIRect{
          .x = 220.0f,
          .y = 120.0f,
          .width = 320.0f,
          .height = 240.0f,
      },
      viewport
  );
  EXPECT_FLOAT_EQ(shifted.x, 80.0f);
  EXPECT_FLOAT_EQ(shifted.y, 60.0f);
  EXPECT_FLOAT_EQ(shifted.width, 320.0f);
  EXPECT_FLOAT_EQ(shifted.height, 240.0f);

  const UIRect shrunk = clamp_rect_to_bounds(
      UIRect{
          .x = 140.0f,
          .y = 20.0f,
          .width = 540.0f,
          .height = 340.0f,
      },
      viewport
  );
  EXPECT_FLOAT_EQ(shrunk.x, 0.0f);
  EXPECT_FLOAT_EQ(shrunk.y, 0.0f);
  EXPECT_FLOAT_EQ(shrunk.width, viewport.width);
  EXPECT_FLOAT_EQ(shrunk.height, viewport.height);
}

TEST(UIFoundationsTest, ClampsMovedPanelsToParentBounds) {
  const UIRect parent_bounds{
      .x = 32.0f, .y = 24.0f, .width = 640.0f, .height = 360.0f
  };

  const UIRect moved_left_top = clamp_rect_to_bounds(
      UIRect{.x = -48.0f, .y = -12.0f, .width = 240.0f, .height = 180.0f},
      parent_bounds
  );
  EXPECT_FLOAT_EQ(moved_left_top.x, parent_bounds.x);
  EXPECT_FLOAT_EQ(moved_left_top.y, parent_bounds.y);
  EXPECT_FLOAT_EQ(moved_left_top.width, 240.0f);
  EXPECT_FLOAT_EQ(moved_left_top.height, 180.0f);

  const UIRect moved_right_bottom = clamp_rect_to_bounds(
      UIRect{.x = 580.0f, .y = 260.0f, .width = 240.0f, .height = 180.0f},
      parent_bounds
  );
  EXPECT_FLOAT_EQ(
      moved_right_bottom.x, parent_bounds.right() - moved_right_bottom.width
  );
  EXPECT_FLOAT_EQ(
      moved_right_bottom.y, parent_bounds.bottom() - moved_right_bottom.height
  );
  EXPECT_FLOAT_EQ(moved_right_bottom.width, 240.0f);
  EXPECT_FLOAT_EQ(moved_right_bottom.height, 180.0f);
}

TEST(UIFoundationsTest, MaxContentWidthClampsViewToIntrinsicChildWidth) {
  using namespace dsl;
  using namespace dsl::styles;

  auto document = UIDocument::create();
  UINodeId card = k_invalid_node_id;

  mount(
      *document,
      column("root")
          .style(fill(), items_start())
          .children(
              view("card")
                  .bind(card)
                  .style(
                      width(px(280.0f)),
                      max_width(max_content()),
                      padding(12.0f)
                  )
                  .children(slider(0.5f, 0.0f, 1.0f, "density"))
          )
  );

  layout_document(
      *document,
      UILayoutContext{
          .viewport_size = glm::vec2(1200.0f, 800.0f),
          .default_font_size = 16.0f,
      }
  );

  const auto *card_node = document->node(card);
  ASSERT_NE(card_node, nullptr);
  EXPECT_FLOAT_EQ(card_node->layout.measured_size.x, 204.0f);
}

} // namespace
} // namespace astralix::ui
