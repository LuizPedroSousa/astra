#include "arena.hpp"
#include "exceptions/base-exception.hpp"
#include "faker-cxx/faker.h"
#include "log.hpp"
#include <gtest/gtest.h>

using namespace astralix;

TEST(ElasticArenaTest, EnsureNoPoolIncreasehenBlockEqualsCapacity) {
  const auto page_size = faker::number::integer(32, 1024);
  astralix::ElasticArena elastic_arena(page_size);

  const auto block_size = page_size;

  // Initial memory should be 0
  EXPECT_EQ(elastic_arena.allocated_memory(), 0);

  elastic_arena.allocate(block_size);

  EXPECT_EQ(elastic_arena.allocated_memory(), block_size);

  EXPECT_EQ(elastic_arena.pool_size(), 1);
}

TEST(ElasticArenaTest, EnsurePoolOverflowWhenIncreasedCapacityIsFull) {
  const auto page_size = faker::number::integer(1, 32);

  auto max_capacity_increase = faker::number::integer(1, 12);

  astralix::ElasticArena elastic_arena(page_size, max_capacity_increase);

  size_t block_size = page_size * 2;

  EXPECT_EQ(elastic_arena.allocated_memory(), 0);

  for (size_t i = 0; i <= max_capacity_increase; i++) {

    if (elastic_arena.capacity_increase_count() >=
        elastic_arena.max_capacity_increase()) {
      EXPECT_THROW(
          elastic_arena.allocate(elastic_arena.capacity() + block_size),
          BaseException);
      return;
    }

    size_t last_capacity = elastic_arena.capacity();

    elastic_arena.allocate(elastic_arena.capacity() + block_size);

    EXPECT_GT(elastic_arena.capacity(), last_capacity);
  }
}

TEST(ElasticArenaTest, PoolIncreaseWhenBlockGreatherThanCapacity) {
  const auto page_size = faker::number::integer(32, 1024);

  auto max_capacity_increase = faker::number::integer(page_size);

  astralix::ElasticArena elastic_arena(page_size, max_capacity_increase);

  size_t block_size = page_size * 2;

  EXPECT_EQ(elastic_arena.allocated_memory(), 0);

  for (size_t i = 1; i <= max_capacity_increase; i++) {

    elastic_arena.allocate(page_size * 2);

    EXPECT_EQ(elastic_arena.allocated_memory(), block_size * i);
  }

  elastic_arena.reset();

  EXPECT_EQ(elastic_arena.allocated_memory(), 0);
  EXPECT_EQ(elastic_arena.pool_size(), 1);
  EXPECT_EQ(elastic_arena.capacity(), elastic_arena.page_size());
}

TEST(ElasticArenaTest, ReusesLargerFreeBlockWhenFrontBlockIsTooSmall) {
  astralix::ElasticArena elastic_arena(40);

  auto *first = elastic_arena.allocate(8);
  auto *middle = elastic_arena.allocate(8);
  auto *tail = elastic_arena.allocate(24);

  elastic_arena.release(first);
  elastic_arena.release(tail);

  EXPECT_NO_THROW({
    auto *reused = elastic_arena.allocate(16);
    ASSERT_NE(reused, nullptr);
    EXPECT_EQ(reused->pool, tail->pool);
  });

  elastic_arena.release(middle);
}

TEST(ElasticArenaTest, ResetClearsCapacityIncreaseCount) {
  astralix::ElasticArena elastic_arena(16, 4);

  auto *block = elastic_arena.allocate(64);

  EXPECT_GT(elastic_arena.capacity_increase_count(), 0u);
  EXPECT_GT(elastic_arena.capacity(), elastic_arena.page_size());

  elastic_arena.release(block);
  elastic_arena.reset();

  EXPECT_EQ(elastic_arena.capacity_increase_count(), 0u);
  EXPECT_EQ(elastic_arena.capacity(), elastic_arena.page_size());
  EXPECT_EQ(elastic_arena.allocated_memory(), 0u);
}

TEST(ElasticStackArenaTest, SinglePushPop) {
  const auto page_size = faker::number::integer(32, 1024);
  astralix::ElasticArena elastic_arena(page_size);
  astralix::ElasticStackArena stack_arena(elastic_arena);

  const auto block_size = faker::number::integer(32, page_size);

  // Initial memory should be 0
  EXPECT_EQ(stack_arena.allocated_memory(), 0);

  // Allocate a block
  stack_arena.push(block_size);
  EXPECT_EQ(stack_arena.allocated_memory(), block_size);

  // Pop the block
  stack_arena.pop();
  EXPECT_EQ(stack_arena.allocated_memory(), 0);

  EXPECT_EQ(elastic_arena.pool_size(), 1);
}

TEST(ElasticStackArenaTest, MultiPushWithLinearPop) {
  const auto page_size = faker::number::integer(32, 1024);

  astralix::ElasticArena elastic_arena(page_size);

  astralix::ElasticStackArena stack_arena(elastic_arena);

  EXPECT_EQ(elastic_arena.allocated_memory(),
            0); // ensure no initial memory push)
  //
  EXPECT_EQ(stack_arena.allocated_memory(), elastic_arena.allocated_memory());

  const auto block_size = faker::number::integer(32, 1024);

  int allocations = 0;

  for (int i = block_size; i <= page_size; i *= 2) {
    stack_arena.push(block_size);
    allocations += block_size;

    EXPECT_EQ(stack_arena.allocated_memory(), allocations);
  }

  EXPECT_EQ(elastic_arena.pool_size(), allocations > page_size ? 2 : 1);

  for (int i = allocations; i >= block_size; i -= block_size) {
    EXPECT_EQ(stack_arena.allocated_memory(), i);

    stack_arena.pop();
  }

  EXPECT_EQ(stack_arena.allocated_memory(), 0);
}

TEST(ElasticStackArenaTest, MultiplePushWithPartialPop) {
  const auto page_size = faker::number::integer(32, 1024);

  const auto max_allocations = faker::number::integer(32, page_size);
  const auto min_allocations = faker::number::integer(32, max_allocations);

  astralix::ElasticArena elastic_arena(page_size, max_allocations);
  astralix::ElasticStackArena stack_arena(elastic_arena);

  const int num_blocks = max_allocations - min_allocations + 1;
  const int num_pops = num_blocks / 2; // Pop half the blocks

  size_t total_allocated = 0;
  std::vector<size_t> block_sizes;

  // Allocate blocks and track total size
  for (int i = min_allocations; i <= max_allocations; i++) {
    stack_arena.push(i);
    total_allocated += i;
    block_sizes.push_back(i);
  }

  EXPECT_EQ(stack_arena.allocated_memory(), total_allocated);

  // Pop half the blocks
  size_t popped_size = 0;
  for (int i = 0; i < num_pops; i++) {
    stack_arena.pop();
    popped_size += block_sizes.back();
    block_sizes.pop_back();
  }

  EXPECT_EQ(stack_arena.allocated_memory(), total_allocated - popped_size);
}

TEST(ElasticStackArenaTest, ClearReleasesAllBlocks) {
  astralix::ElasticArena elastic_arena(64);
  astralix::ElasticStackArena stack_arena(elastic_arena);

  stack_arena.push(16);
  stack_arena.push(16);
  stack_arena.push(16);

  ASSERT_GT(stack_arena.allocated_memory(), 0u);

  stack_arena.clear();

  EXPECT_EQ(stack_arena.allocated_memory(), 0u);
  EXPECT_EQ(elastic_arena.allocated_memory(), 0u);
}

TEST(Str, EnsureEquality) {
  const auto sso_text_length = faker::number::integer(1, 22);
  const auto sso_text = faker::string::alpha(sso_text_length);

  auto arena = ElasticArena(KB(1));

  Str sso_str = Str::from_initializer({arena, sso_text.c_str()});

  EXPECT_EQ(sso_str.is_sso(), true);

  EXPECT_EQ(sso_str, sso_text.c_str());
  EXPECT_EQ(sso_str, sso_text);

  sso_str.release();

  EXPECT_EQ(sso_str.size(), 0);
  EXPECT_EQ(sso_str.allocated_memory(), 0);

  const auto text = faker::lorem::sentences();

  Str str = Str::from_initializer({arena, text.c_str()});

  EXPECT_EQ(str, text.c_str());
  EXPECT_EQ(str, text);
}

TEST(Str, SmallStringShouldHitSSO) {
  const auto sso_text_length = faker::number::integer(1, 22);
  const auto text = faker::string::alpha(sso_text_length);

  auto arena = ElasticArena(KB(1));

  Str str = Str::from_initializer({arena, text.c_str()});

  EXPECT_EQ(str.size(), sso_text_length);
  EXPECT_EQ(str.is_sso(), true);

  EXPECT_TRUE(str == text.c_str());
}

TEST(Str, LargeStringShouldNotHitSSO) {
  const auto text = faker::lorem::sentences();

  auto arena = ElasticArena(KB(1));

  Str str = Str::from_initializer({arena, text.c_str()});

  EXPECT_EQ(str.size(), text.size());
  EXPECT_EQ(str.is_sso(), false);

  EXPECT_TRUE(str == text.c_str());
}

TEST(Str, AppendWithSmallStringShouldHitSSO) {
  const auto sso_text_length = faker::number::integer(1, 22);
  const auto text = faker::string::alpha(sso_text_length);

  auto arena = ElasticArena(KB(1));
  Str str = Str::from_initializer({arena, text.c_str()});

  Str other(arena);

  EXPECT_TRUE(str.is_sso());
  EXPECT_EQ(other.size(), 0);

  const auto sub_length = faker::number::integer(1, sso_text_length);
  other.append(str, sub_length);

  EXPECT_EQ(other.size(), sub_length);

  EXPECT_TRUE(other.is_sso());

  // ensure original string remains unchanged
  EXPECT_EQ(str.size(), sso_text_length);
  EXPECT_EQ(str, text);

  // ensure the data is copied rather than a pointer
  str.release();

  EXPECT_EQ(str.size(), 0);
  EXPECT_EQ(str.allocated_memory(), 0);

  EXPECT_EQ(other.size(), sub_length);
}

TEST(Str, AppendWithLargeStringShouldNotHitSSO) {
  const auto text = faker::lorem::sentences();

  auto arena = ElasticArena(KB(1));

  Str str = Str::from_initializer({arena, text.c_str()});

  Str other(arena);

  EXPECT_FALSE(str.is_sso());
  EXPECT_EQ(other.size(), 0);

  const auto sub_length = faker::number::integer(23, (int)text.size());
  other.append(str, sub_length);

  EXPECT_EQ(other.size(), sub_length);

  EXPECT_FALSE(other.is_sso());

  // ensure original string remains unchanged
  EXPECT_EQ(str.size(), text.size());
  EXPECT_EQ(str, text);

  // ensure the data is copied rather than a pointer
  str.release();

  EXPECT_EQ(str.size(), 0);
  EXPECT_EQ(str.allocated_memory(), 0);

  auto sub = text.substr(0, sub_length);
  EXPECT_EQ(other.size(), sub_length);
  EXPECT_EQ(other, sub);
}

TEST(Str, AppendCStr) {
  const auto text = faker::lorem::sentences();

  auto arena = ElasticArena(KB(1));

  Str str = Str::from_initializer({arena, text.c_str()});
  Str other(arena);

  EXPECT_FALSE(str.is_sso());
  EXPECT_EQ(other.size(), 0);

  const auto sub_length = faker::number::integer(23, (int)text.size());
  other.append(str.c_str(), sub_length);

  EXPECT_EQ(other.size(), sub_length);

  EXPECT_FALSE(other.is_sso());

  // ensure original string remains unchanged
  EXPECT_EQ(str.size(), text.size());
  EXPECT_EQ(str, text);

  // ensure the data is copied rather than a pointer
  str.release();

  EXPECT_EQ(str.size(), 0);
  EXPECT_EQ(str.allocated_memory(), 0);

  auto sub = text.substr(0, sub_length);
  EXPECT_EQ(other.size(), sub_length);
  EXPECT_EQ(other, sub);
}

TEST(Str, AppendLargeStringToEmptyStr) {
  auto arena = ElasticArena(KB(1));

  Str empty_str(arena);

  const auto large_text = faker::lorem::sentences();
  empty_str.append(large_text.c_str(), large_text.size());

  EXPECT_FALSE(empty_str.is_sso());
  EXPECT_EQ(empty_str.size(), large_text.size());
  EXPECT_EQ(empty_str, large_text);
}

TEST(Str, EmptyStringAppend) {
  auto arena = ElasticArena(KB(1));

  Str str("", arena);
  EXPECT_TRUE(str.is_sso());
  EXPECT_EQ(str.size(), 0);
  EXPECT_STREQ(str.c_str(), "");

  const auto text = faker::string::alpha(10);
  str.append(text.c_str(), text.size());
  EXPECT_TRUE(str.is_sso());
  EXPECT_EQ(str, text);
}

TEST(Str, AppendSmallToSmallWithinSSO) {
  auto arena = ElasticArena(KB(1));

  const auto str_text = faker::string::alpha(10);
  const auto other_text = faker::string::alpha(10); // 10 + 10 = 20 <= 22

  Str str(str_text.c_str(), arena);
  Str other(other_text.c_str(), arena);

  str.append(other, other.size());

  EXPECT_TRUE(str.is_sso());
  EXPECT_EQ(str.size(), str_text.size() + other_text.size());
  EXPECT_EQ(str, str_text + other_text);
}

TEST(Str, AppendSmallToSmallExceedingSSO) {
  auto arena = ElasticArena(KB(1));

  const auto str_text = faker::string::alpha(15);
  const auto other_text = faker::string::alpha(10); // 15 + 10 = 25 > 22

  Str str(str_text.c_str(), arena);
  Str other(other_text.c_str(), arena);

  str.append(other, other.size());

  EXPECT_FALSE(str.is_sso());
  EXPECT_EQ(str.size(), str_text.size() + other_text.size());
  EXPECT_EQ(str, str_text + other_text);
}

TEST(Str, NullTermination) {
  auto arena = ElasticArena(KB(1));

  const auto small_text = faker::string::alpha(10);
  Str small_str(small_text.c_str(), arena);
  EXPECT_EQ(std::strlen(small_str.c_str()), small_text.size());
  EXPECT_EQ(small_str.c_str()[small_text.size()], '\0');

  const auto large_text = faker::lorem::sentences();
  Str large_str(large_text.c_str(), arena);
  EXPECT_EQ(std::strlen(large_str.c_str()), large_text.size());
  EXPECT_EQ(large_str.c_str()[large_text.size()], '\0');
}

TEST(Str, AppendLargeToSmall) {
  auto arena = ElasticArena(KB(1));

  const auto small_text = faker::string::alpha(10);
  const auto large_text = faker::lorem::sentences();

  Str small_str(small_text.c_str(), arena);
  Str large_str(large_text.c_str(), arena);

  small_str.append(large_str, large_str.size());
  EXPECT_FALSE(small_str.is_sso());
  EXPECT_EQ(small_str.size(), small_text.size() + large_text.size());
  EXPECT_EQ(small_str, small_text + large_text);
}

TEST(Str, AppendSmallToLarge) {
  auto arena = ElasticArena(KB(1));

  const auto large_text = faker::lorem::sentences();
  const auto small_text = faker::string::alpha(10);

  Str large_str(large_text.c_str(), arena);
  Str small_str(small_text.c_str(), arena);

  large_str.append(small_str, small_str.size());
  EXPECT_FALSE(large_str.is_sso());
  EXPECT_EQ(large_str.size(), large_text.size() + small_text.size());
  EXPECT_EQ(large_str, large_text + small_text);
}

TEST(Str, AppendLargeToLarge) {
  auto arena = ElasticArena(KB(1));

  const auto str_text = faker::lorem::sentences();
  const auto other_text = faker::lorem::sentences();

  Str str(str_text.c_str(), arena);
  Str other(other_text.c_str(), arena);

  str.append(other, other.size());

  EXPECT_FALSE(str.is_sso());
  EXPECT_EQ(str.size(), str_text.size() + other_text.size());
  EXPECT_EQ(str, str_text + other_text);
}

TEST(Str, OperatorPlusEqualStr) {
  auto arena = ElasticArena(KB(1));

  const auto str_text = faker::string::alpha(10);
  const auto other_text = faker::string::alpha(10); // 10 + 10 = 20 <= 22

  Str str(str_text.c_str(), arena);
  Str other(other_text.c_str(), arena);
  str += other;

  EXPECT_TRUE(str.is_sso());
  EXPECT_EQ(str.size(), str_text.size() + other_text.size());

  EXPECT_EQ(str, str_text + other_text);
}

TEST(Str, OperatorPlusEqualCStr) {
  auto arena = ElasticArena(KB(1));

  const auto str_text = faker::string::alpha(10);
  const auto other_text = faker::string::alpha(10);

  Str str(str_text.c_str(), arena);
  str += other_text.c_str();

  EXPECT_TRUE(str.is_sso()); // 10 + 10 = 20 <= 22
  EXPECT_EQ(str.size(), str_text.size() + std::strlen(other_text.c_str()));
  EXPECT_EQ(str, str_text + other_text);
}

TEST(Str, AssignmentWithCStr) {
  auto arena = ElasticArena(KB(1));

  const auto text = faker::lorem::sentences();

  Str str(arena);
  str = text.c_str();

  EXPECT_FALSE(str.is_sso());

  EXPECT_EQ(str.size(), std::strlen(text.c_str()));

  EXPECT_EQ(str, text);
}

TEST(Str, ReleaseSSOString) {
  auto arena = ElasticArena(KB(1));

  const auto text = faker::string::alpha(10);

  Str str(text.c_str(), arena);

  EXPECT_TRUE(str.is_sso());

  str.release();
  EXPECT_EQ(str.size(), 0);
  EXPECT_EQ(str.allocated_memory(), 0);
}

TEST(Str, ReleaseNonSSOString) {
  auto arena = ElasticArena(KB(1));

  const auto text = faker::lorem::sentences();
  Str str(text.c_str(), arena);
  EXPECT_FALSE(str.is_sso());
  str.release();
  EXPECT_EQ(str.size(), 0);
  EXPECT_EQ(str.allocated_memory(), 0);
}

TEST(Str, SelfAppend) {
  auto arena = ElasticArena(KB(1));

  const auto text = faker::string::alpha(10);
  Str str(text.c_str(), arena);
  str.append(str, str.size());

  EXPECT_TRUE(str.is_sso()); // 10 + 10 = 20 <= 22
  EXPECT_EQ(str.size(), text.size() * 2);
  EXPECT_EQ(str, text + text);
}
