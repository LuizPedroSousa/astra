#include "disclosure.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, DisclosureHelperCreatesExpectedStructureAndTogglesBody) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  document->set_root(root);

  const UIDisclosureNodes disclosure = create_disclosure(
      *document, root, UIDisclosureOptions{.open = false}
  );

  const auto *root_node = document->node(disclosure.root);
  const auto *header_node = document->node(disclosure.header_button);
  const auto *body_node = document->node(disclosure.body);
  ASSERT_NE(root_node, nullptr);
  ASSERT_NE(header_node, nullptr);
  ASSERT_NE(body_node, nullptr);

  ASSERT_EQ(root_node->children.size(), 2u);
  EXPECT_EQ(root_node->children[0], disclosure.header_button);
  EXPECT_EQ(root_node->children[1], disclosure.body);
  EXPECT_EQ(header_node->parent, disclosure.root);
  EXPECT_EQ(body_node->parent, disclosure.root);
  EXPECT_TRUE(header_node->focusable);
  EXPECT_FALSE(disclosure_open(*document, disclosure));

  set_disclosure_open(*document, disclosure, true);
  EXPECT_TRUE(disclosure_open(*document, disclosure));

  set_disclosure_open(*document, disclosure, false);
  EXPECT_FALSE(disclosure_open(*document, disclosure));
}

} // namespace
} // namespace astralix::ui
