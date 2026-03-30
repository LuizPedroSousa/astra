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
      column("root").children(
          render_image_view(
              RenderImageResource::ShadowMap,
              RenderImageAspect::Depth,
              "viewport"
          )
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

} // namespace
} // namespace astralix::ui
