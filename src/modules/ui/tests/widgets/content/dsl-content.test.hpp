#include "document/document.hpp"
#include "dsl.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, RenderImageViewDocumentAndDslStoreRendererExportKeys) {
  using namespace dsl;

  auto document = UIDocument::create();
  UINodeId viewport = k_invalid_node_id;

  mount(
      *document,
      column().children(
          render_image_view(RenderImageResource::ShadowMap, RenderImageAspect::Depth)
              .bind(viewport)
      )
  );

  const auto *viewport_node = document->node(viewport);
  ASSERT_NE(viewport_node, nullptr);
  EXPECT_EQ(viewport_node->type, NodeType::RenderImageView);
  ASSERT_TRUE(viewport_node->render_image_key.has_value());
  EXPECT_EQ(
      viewport_node->render_image_key->resource,
      RenderImageResource::ShadowMap
  );
  EXPECT_EQ(
      viewport_node->render_image_key->aspect,
      RenderImageAspect::Depth
  );
}

TEST(UIFoundationsTest, ImageNodesKeepArbitraryImageResourceIds) {
  using namespace dsl;

  auto document = UIDocument::create();
  UINodeId icon = k_invalid_node_id;

  mount(*document, column().children(image("svg::icons::close").bind(icon)));

  const auto *icon_node = document->node(icon);
  ASSERT_NE(icon_node, nullptr);
  EXPECT_EQ(icon_node->type, NodeType::Image);
  EXPECT_EQ(icon_node->texture_id, "svg::icons::close");
}

} // namespace
} // namespace astralix::ui
