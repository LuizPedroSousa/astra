#include "world.hpp"
#include "exceptions/base-exception.hpp"
#include <gtest/gtest.h>

namespace astralix::ecs {
namespace {

struct Position {
  int x = 0;
};

struct Velocity {
  int x = 0;
};

TEST(SignatureTest, ContainsRequiredBitsAcrossWordBoundaries) {
  Signature full;
  full.set(component_type_id<Position>());
  full.set(130u);

  Signature required;
  required.set(component_type_id<Position>());

  EXPECT_TRUE(full.contains(required));
  EXPECT_TRUE(full.test(component_type_id<Position>()));
  EXPECT_TRUE(full.test(130u));

  required.set(131u);
  EXPECT_FALSE(full.contains(required));
}

TEST(WorldTest, SpawnStoresMetadataAndSupportsQueries) {
  World world;
  auto entity = world.spawn("player");

  entity.emplace<Position>(Position{.x = 42});
  entity.emplace<Velocity>(Velocity{.x = 3});

  ASSERT_TRUE(entity.exists());
  EXPECT_EQ(entity.name(), "player");
  EXPECT_TRUE(entity.active());
  EXPECT_TRUE(entity.has<Position>());
  ASSERT_NE(entity.get<Position>(), nullptr);
  EXPECT_EQ(entity.get<Position>()->x, 42);

  int count = 0;
  world.each<Position, Velocity>(
      [&](EntityID, Position &position, Velocity &velocity) {
        count++;
        EXPECT_EQ(position.x, 42);
        EXPECT_EQ(velocity.x, 3);
      });

  EXPECT_EQ(count, 1);
}

TEST(WorldTest, RemovingComponentMigratesEntityToMatchingArchetype) {
  World world;
  auto entity = world.spawn("migrating");

  entity.emplace<Position>(Position{.x = 7});
  entity.emplace<Velocity>(Velocity{.x = 9});
  entity.erase<Velocity>();

  EXPECT_TRUE(entity.has<Position>());
  EXPECT_FALSE(entity.has<Velocity>());
  EXPECT_EQ(world.count<Position>(), 1u);
  EXPECT_EQ((world.count<Position, Velocity>()), 0u);
}

TEST(WorldTest, EntityRefRejectsInvalidMutation) {
  World world;
  auto entity = world.spawn("transient");
  EntityID entity_id = entity.id();
  world.destroy(entity_id);

  EXPECT_FALSE(world.entity(entity_id).exists());
  EXPECT_THROW(world.entity(entity_id).set_name("broken"), BaseException);
}

TEST(WorldTest, EnsureReusesExistingEntityIdAndUpdatesMetadata) {
  World world;
  EntityID entity_id;

  auto first = world.ensure(entity_id, "first", true);
  auto second = world.ensure(entity_id, "second", false);

  EXPECT_EQ(first.id(), second.id());
  EXPECT_EQ(second.name(), "second");
  EXPECT_FALSE(second.active());
  EXPECT_EQ(world.size(), 1u);
}

TEST(CommandBufferTest, AppliesDeferredOperationsInOrder) {
  World world;
  auto commands = world.commands();
  auto pending = commands.spawn("buffered");
  commands.emplace<Position>(pending.id(), Position{.x = 5});
  commands.set_active(pending.id(), false);
  commands.apply();

  auto entity = world.entity(pending.id());
  ASSERT_TRUE(entity.exists());
  ASSERT_NE(entity.get<Position>(), nullptr);
  EXPECT_EQ(entity.get<Position>()->x, 5);
  EXPECT_FALSE(entity.active());
}

} // namespace
} // namespace astralix::ecs
