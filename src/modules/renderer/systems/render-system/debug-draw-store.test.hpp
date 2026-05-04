#include "managers/debug-draw-store.hpp"

#include <gtest/gtest.h>

namespace astralix {
namespace {

TEST(DebugDrawStoreTest, FrameLocalCommandsAreClearedAfterAdvance) {
  auto store = debug_draw();
  store->clear();

  store->line(
      glm::vec3(0.0f),
      glm::vec3(1.0f, 0.0f, 0.0f),
      glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
      0.0f
  );
  store->point(glm::vec3(0.0f), 8.0f, glm::vec4(1.0f), 0.0f);

  ASSERT_FALSE(store->empty());
  store->advance(1.0 / 60.0);

  EXPECT_TRUE(store->empty());
}

TEST(DebugDrawStoreTest, PersistentCommandsExpireOnlyAfterDurationElapses) {
  auto store = debug_draw();
  store->clear();

  DebugDrawStyle style;
  style.duration_seconds = 0.5f;
  store->line(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), style);

  ASSERT_EQ(store->lines().size(), 1u);
  store->advance(0.2);
  ASSERT_EQ(store->lines().size(), 1u);
  EXPECT_GT(store->lines().front().style.duration_seconds, 0.0f);

  store->advance(0.31);
  EXPECT_TRUE(store->lines().empty());
}

TEST(DebugDrawStoreTest, CategoryToggleDefaultsToEnabled) {
  auto store = debug_draw();
  store->clear();
  store->set_category_enabled("math", false);
  EXPECT_FALSE(store->category_enabled("math"));
  EXPECT_TRUE(store->category_enabled("physics"));

  store->set_category_enabled("math", true);
  EXPECT_TRUE(store->category_enabled("math"));
}

} // namespace
} // namespace astralix
