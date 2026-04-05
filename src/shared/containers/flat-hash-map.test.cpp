#include "flat-hash-map.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace astralix {
namespace {

TEST(FlatHashMapTest, DefaultConstructedMapIsEmpty) {
  FlatHashMap<int, int> map;

  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0u);
  EXPECT_EQ(map.begin(), map.end());
}

TEST(FlatHashMapTest, SubscriptInsertsAndRetrieves) {
  FlatHashMap<int, int> map;

  map[1] = 10;
  map[2] = 20;
  map[3] = 30;

  EXPECT_EQ(map.size(), 3u);
  EXPECT_EQ(map[1], 10);
  EXPECT_EQ(map[2], 20);
  EXPECT_EQ(map[3], 30);
}

TEST(FlatHashMapTest, SubscriptOverwritesExistingValue) {
  FlatHashMap<int, int> map;

  map[42] = 100;
  EXPECT_EQ(map[42], 100);

  map[42] = 200;
  EXPECT_EQ(map[42], 200);
  EXPECT_EQ(map.size(), 1u);
}

TEST(FlatHashMapTest, FindReturnsEndForMissingKey) {
  FlatHashMap<int, int> map;

  EXPECT_EQ(map.find(99), map.end());

  map[1] = 10;
  EXPECT_EQ(map.find(99), map.end());
}

TEST(FlatHashMapTest, FindReturnsIteratorToExistingKey) {
  FlatHashMap<int, int> map;

  map[5] = 50;
  map[10] = 100;

  auto iterator = map.find(5);
  ASSERT_NE(iterator, map.end());
  EXPECT_EQ(iterator->first, 5);
  EXPECT_EQ(iterator->second, 50);

  iterator = map.find(10);
  ASSERT_NE(iterator, map.end());
  EXPECT_EQ(iterator->first, 10);
  EXPECT_EQ(iterator->second, 100);
}

TEST(FlatHashMapTest, ConstFindWorksOnConstMap) {
  FlatHashMap<int, int> map;
  map[1] = 10;
  map[2] = 20;

  const auto &const_map = map;

  auto iterator = const_map.find(1);
  ASSERT_NE(iterator, const_map.end());
  EXPECT_EQ(iterator->first, 1);
  EXPECT_EQ(iterator->second, 10);

  EXPECT_EQ(const_map.find(999), const_map.end());
}

TEST(FlatHashMapTest, EraseRemovesExistingKey) {
  FlatHashMap<int, int> map;

  map[1] = 10;
  map[2] = 20;
  map[3] = 30;

  map.erase(2);

  EXPECT_EQ(map.size(), 2u);
  EXPECT_EQ(map.find(2), map.end());
  EXPECT_EQ(map[1], 10);
  EXPECT_EQ(map[3], 30);
}

TEST(FlatHashMapTest, EraseNonExistentKeyIsNoOp) {
  FlatHashMap<int, int> map;

  map[1] = 10;
  map.erase(999);

  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(map[1], 10);
}

TEST(FlatHashMapTest, EraseOnEmptyMapIsNoOp) {
  FlatHashMap<int, int> map;

  map.erase(1);

  EXPECT_TRUE(map.empty());
}

TEST(FlatHashMapTest, ClearResetsMapToEmpty) {
  FlatHashMap<int, int> map;

  map[1] = 10;
  map[2] = 20;
  map[3] = 30;

  map.clear();

  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0u);
  EXPECT_EQ(map.find(1), map.end());
  EXPECT_EQ(map.find(2), map.end());
  EXPECT_EQ(map.find(3), map.end());
}

TEST(FlatHashMapTest, ClearAllowsReinsertion) {
  FlatHashMap<int, int> map;

  map[1] = 10;
  map.clear();

  map[1] = 99;
  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(map[1], 99);
}

TEST(FlatHashMapTest, ReservePreallocatesWithoutChangingSize) {
  FlatHashMap<int, int> map;

  map.reserve(200);

  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0u);
}

TEST(FlatHashMapTest, IteratorVisitsAllEntries) {
  FlatHashMap<int, int> map;

  map[1] = 10;
  map[2] = 20;
  map[3] = 30;
  map[4] = 40;

  std::vector<std::pair<int, int>> entries;
  for (auto [key, value] : map) {
    entries.emplace_back(key, value);
  }

  EXPECT_EQ(entries.size(), 4u);

  std::sort(entries.begin(), entries.end());
  EXPECT_EQ(entries[0], std::make_pair(1, 10));
  EXPECT_EQ(entries[1], std::make_pair(2, 20));
  EXPECT_EQ(entries[2], std::make_pair(3, 30));
  EXPECT_EQ(entries[3], std::make_pair(4, 40));
}

TEST(FlatHashMapTest, ConstIteratorVisitsAllEntries) {
  FlatHashMap<int, int> map;
  map[10] = 100;
  map[20] = 200;

  const auto &const_map = map;

  std::vector<std::pair<int, int>> entries;
  for (auto [key, value] : const_map) {
    entries.emplace_back(key, value);
  }

  EXPECT_EQ(entries.size(), 2u);

  std::sort(entries.begin(), entries.end());
  EXPECT_EQ(entries[0], std::make_pair(10, 100));
  EXPECT_EQ(entries[1], std::make_pair(20, 200));
}

TEST(FlatHashMapTest, MutateValueThroughIterator) {
  FlatHashMap<int, int> map;
  map[1] = 10;

  auto iterator = map.find(1);
  ASSERT_NE(iterator, map.end());

  iterator->second = 999;
  EXPECT_EQ(map[1], 999);
}

TEST(FlatHashMapTest, StringKeys) {
  FlatHashMap<std::string, int> map;

  map["alpha"] = 1;
  map["beta"] = 2;
  map["gamma"] = 3;

  EXPECT_EQ(map.size(), 3u);
  EXPECT_EQ(map["alpha"], 1);
  EXPECT_EQ(map["beta"], 2);
  EXPECT_EQ(map["gamma"], 3);

  map.erase("beta");
  EXPECT_EQ(map.size(), 2u);
  EXPECT_EQ(map.find("beta"), map.end());
  EXPECT_EQ(map["alpha"], 1);
  EXPECT_EQ(map["gamma"], 3);
}

TEST(FlatHashMapTest, GrowthPreservesAllEntries) {
  FlatHashMap<int, int> map;

  constexpr int count = 500;
  for (int index = 0; index < count; ++index) {
    map[index] = index * 10;
  }

  EXPECT_EQ(map.size(), static_cast<size_t>(count));

  for (int index = 0; index < count; ++index) {
    auto iterator = map.find(index);
    ASSERT_NE(iterator, map.end()) << "key " << index << " missing after growth";
    EXPECT_EQ(iterator->second, index * 10);
  }
}

TEST(FlatHashMapTest, EraseAndReinsert) {
  FlatHashMap<int, int> map;

  map[1] = 10;
  map[2] = 20;
  map[3] = 30;

  map.erase(2);
  EXPECT_EQ(map.find(2), map.end());

  map[2] = 200;
  EXPECT_EQ(map.size(), 3u);
  EXPECT_EQ(map[2], 200);
}

TEST(FlatHashMapTest, EraseAllEntries) {
  FlatHashMap<int, int> map;

  for (int index = 0; index < 50; ++index) {
    map[index] = index;
  }

  for (int index = 0; index < 50; ++index) {
    map.erase(index);
  }

  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0u);

  for (int index = 0; index < 50; ++index) {
    EXPECT_EQ(map.find(index), map.end());
  }
}

TEST(FlatHashMapTest, CollidingKeysAreHandledCorrectly) {
  struct ConstantHash {
    size_t operator()(int) const { return 42u; }
  };

  FlatHashMap<int, int, ConstantHash> map;

  map[1] = 10;
  map[2] = 20;
  map[3] = 30;

  EXPECT_EQ(map.size(), 3u);
  EXPECT_EQ(map[1], 10);
  EXPECT_EQ(map[2], 20);
  EXPECT_EQ(map[3], 30);

  map.erase(2);

  EXPECT_EQ(map.size(), 2u);
  EXPECT_EQ(map.find(2), map.end());
  EXPECT_EQ(map[1], 10);
  EXPECT_EQ(map[3], 30);
}

TEST(FlatHashMapTest, EraseMiddleOfCollisionChainPreservesRest) {
  struct ConstantHash {
    size_t operator()(int) const { return 0u; }
  };

  FlatHashMap<int, int, ConstantHash> map;

  map[1] = 10;
  map[2] = 20;
  map[3] = 30;
  map[4] = 40;
  map[5] = 50;

  map.erase(3);

  EXPECT_EQ(map.size(), 4u);
  EXPECT_EQ(map[1], 10);
  EXPECT_EQ(map[2], 20);
  EXPECT_EQ(map.find(3), map.end());
  EXPECT_EQ(map[4], 40);
  EXPECT_EQ(map[5], 50);
}

TEST(FlatHashMapTest, SubscriptDefaultConstructsValue) {
  FlatHashMap<int, int> map;

  int &value = map[99];
  EXPECT_EQ(value, 0);
  EXPECT_EQ(map.size(), 1u);

  value = 42;
  EXPECT_EQ(map[99], 42);
}

TEST(FlatHashMapTest, ReserveReducesRehashingDuringBulkInsert) {
  FlatHashMap<int, int> map;
  map.reserve(1000);

  for (int index = 0; index < 1000; ++index) {
    map[index] = index;
  }

  EXPECT_EQ(map.size(), 1000u);
  for (int index = 0; index < 1000; ++index) {
    EXPECT_EQ(map[index], index);
  }
}

} // namespace
} // namespace astralix
