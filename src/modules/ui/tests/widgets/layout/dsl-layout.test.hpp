#include "document/document.hpp"
#include "dsl.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, RemLengthsPreserveUnitAndDocumentRootFontSize) {
  using namespace dsl;
  using namespace dsl::styles;

  auto document = UIDocument::create();
  UINodeId panel = k_invalid_node_id;

  mount(
      *document,
      dsl::column().style(fill()).children(
          view().bind(panel).style(width(rem(2.0f)), height(rem(1.5f)))
      )
  );

  document->set_root_font_size(20.0f);

  const auto *panel_node = document->node(panel);
  ASSERT_NE(panel_node, nullptr);
  EXPECT_EQ(panel_node->style.width.unit, UILengthUnit::Rem);
  EXPECT_EQ(panel_node->style.height.unit, UILengthUnit::Rem);
  EXPECT_FLOAT_EQ(panel_node->style.width.value, 2.0f);
  EXPECT_FLOAT_EQ(panel_node->style.height.value, 1.5f);
  EXPECT_FLOAT_EQ(document->root_font_size(), 20.0f);
}

TEST(UIFoundationsTest, FreeStyleHelpersCanChainFromAnyEntryPoint) {
  using namespace dsl;
  using namespace dsl::styles;

  auto document = UIDocument::create();
  UINodeId panel = k_invalid_node_id;

  mount(
      *document,
      dsl::column().children(
          view()
              .bind(panel)
              .style(fill_x().padding(14.0f).gap(12.0f).height(px(40.0f)).radius(6.0f))
      )
  );

  const auto *panel_node = document->node(panel);
  ASSERT_NE(panel_node, nullptr);
  EXPECT_EQ(panel_node->style.width.unit, UILengthUnit::Percent);
  EXPECT_FLOAT_EQ(panel_node->style.width.value, 1.0f);
  EXPECT_EQ(panel_node->style.height.unit, UILengthUnit::Pixels);
  EXPECT_FLOAT_EQ(panel_node->style.height.value, 40.0f);
  EXPECT_FLOAT_EQ(panel_node->style.gap, 12.0f);
  EXPECT_FLOAT_EQ(panel_node->style.padding.left, 14.0f);
  EXPECT_FLOAT_EQ(panel_node->style.padding.right, 14.0f);
  EXPECT_FLOAT_EQ(panel_node->style.padding.top, 14.0f);
  EXPECT_FLOAT_EQ(panel_node->style.padding.bottom, 14.0f);
  EXPECT_FLOAT_EQ(panel_node->style.border_radius, 6.0f);
  EXPECT_FLOAT_EQ(panel_node->style.flex_shrink, 0.0f);
}

TEST(UIFoundationsTest, MaxContentLengthHelperPreservesUnit) {
  using namespace dsl;
  using namespace dsl::styles;

  auto document = UIDocument::create();
  UINodeId panel = k_invalid_node_id;

  mount(
      *document,
      dsl::column().children(
          view().bind(panel).style(max_width(max_content()))
      )
  );

  const auto *panel_node = document->node(panel);
  ASSERT_NE(panel_node, nullptr);
  EXPECT_EQ(panel_node->style.max_width.unit, UILengthUnit::MaxContent);
  EXPECT_FLOAT_EQ(panel_node->style.max_width.value, 0.0f);
}

TEST(UIFoundationsTest, DeclarativeDslMountsAndAppendsTrees) {
  using namespace dsl;
  using namespace dsl::styles;

  auto document = UIDocument::create();
  UINodeId root = k_invalid_node_id;
  UINodeId title = k_invalid_node_id;
  UINodeId actions = k_invalid_node_id;
  UINodeId run_button = k_invalid_node_id;
  UINodeId help = k_invalid_node_id;

  const UINodeId mounted_root = mount(
      *document,
      dsl::column()
          .bind(root)
          .style(fill(), padding(8.0f), gap(6.0f))
          .children(
              text("HUD").bind(title),
              dsl::row()
                  .bind(actions)
                  .style(gap(4.0f))
                  .children(button("Run", [] {}).bind(run_button))
          )
  );

  EXPECT_EQ(mounted_root, root);
  EXPECT_EQ(document->root(), root);

  const auto *root_node = document->node(root);
  ASSERT_NE(root_node, nullptr);
  ASSERT_EQ(root_node->children.size(), 2u);
  EXPECT_EQ(root_node->children[0], title);
  EXPECT_EQ(root_node->children[1], actions);

  const UINodeId appended = append(*document, actions, text("Help").bind(help));
  EXPECT_EQ(appended, help);

  const auto *actions_node = document->node(actions);
  ASSERT_NE(actions_node, nullptr);
  ASSERT_EQ(actions_node->children.size(), 2u);
  EXPECT_EQ(actions_node->children[0], run_button);
  EXPECT_EQ(actions_node->children[1], help);
}

TEST(UIFoundationsTest, DeclarativeDslRowColumnAndSpacerSugarMapToStyle) {
  using namespace dsl;

  auto document = UIDocument::create();
  UINodeId root = k_invalid_node_id;
  UINodeId toolbar = k_invalid_node_id;
  UINodeId filler = k_invalid_node_id;

  mount(
      *document,
      dsl::column().bind(root).children(
          dsl::row().bind(toolbar).children(spacer().bind(filler))
      )
  );

  const auto *root_node = document->node(root);
  const auto *toolbar_node = document->node(toolbar);
  const auto *filler_node = document->node(filler);
  ASSERT_NE(root_node, nullptr);
  ASSERT_NE(toolbar_node, nullptr);
  ASSERT_NE(filler_node, nullptr);

  EXPECT_EQ(root_node->style.flex_direction, FlexDirection::Column);
  EXPECT_EQ(toolbar_node->style.flex_direction, FlexDirection::Row);
  EXPECT_FLOAT_EQ(root_node->style.flex_shrink, 0.0f);
  EXPECT_FLOAT_EQ(toolbar_node->style.flex_shrink, 0.0f);
  EXPECT_FLOAT_EQ(filler_node->style.flex_grow, 1.0f);
  EXPECT_FLOAT_EQ(filler_node->style.flex_shrink, 0.0f);
}

TEST(UIFoundationsTest, DeclarativeDslFlexSugarMapsToReactiveDefaults) {
  using namespace dsl;
  using namespace dsl::styles;

  auto document = UIDocument::create();
  UINodeId root = k_invalid_node_id;
  UINodeId body = k_invalid_node_id;

  mount(
      *document,
      dsl::column().bind(root).children(
          view().bind(body).style(flex(1.0f))
      )
  );

  const auto *root_node = document->node(root);
  const auto *body_node = document->node(body);
  ASSERT_NE(root_node, nullptr);
  ASSERT_NE(body_node, nullptr);

  EXPECT_FLOAT_EQ(root_node->style.flex_shrink, 0.0f);
  EXPECT_FLOAT_EQ(body_node->style.flex_grow, 1.0f);
  EXPECT_FLOAT_EQ(body_node->style.flex_shrink, 1.0f);
  EXPECT_EQ(body_node->style.flex_basis.unit, UILengthUnit::Pixels);
  EXPECT_FLOAT_EQ(body_node->style.flex_basis.value, 0.0f);
}

TEST(UIFoundationsTest, DeclarativeDslSupportsDraggablePanels) {
  using namespace dsl;
  using namespace dsl::styles;

  auto document = UIDocument::create();
  UINodeId panel = k_invalid_node_id;
  UINodeId header = k_invalid_node_id;

  mount(
      *document,
      dsl::column().children(
          dsl::column()
              .bind(panel)
              .style(absolute(), draggable(), left(px(24.0f)), top(px(32.0f)))
              .children(
                  dsl::row().bind(header).style(drag_handle()).children(text("Panel"))
              )
      )
  );

  const auto *panel_node = document->node(panel);
  const auto *header_node = document->node(header);
  ASSERT_NE(panel_node, nullptr);
  ASSERT_NE(header_node, nullptr);
  EXPECT_TRUE(panel_node->style.draggable);
  EXPECT_TRUE(header_node->style.drag_handle);
}

TEST(UIFoundationsTest, DeclarativeDslRejectsChildrenOnLeafNodes) {
  using namespace dsl;

  auto document = UIDocument::create();

  EXPECT_ANY_THROW(
      mount(*document, button("Broken", [] {}).child(text("Nope")))
  );
}

} // namespace
} // namespace astralix::ui
