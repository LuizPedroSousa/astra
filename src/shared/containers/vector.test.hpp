#include "vector.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace astralix {
namespace {

struct CountingValue {
  inline static int constructions = 0;
  inline static int destructions = 0;

  int value = 0;

  explicit CountingValue(int next_value) : value(next_value) { ++constructions; }

  CountingValue(const CountingValue &other) : value(other.value) {
    ++constructions;
  }

  CountingValue(CountingValue &&other) noexcept : value(other.value) {
    ++constructions;
    other.value = -1;
  }

  CountingValue &operator=(const CountingValue &) = default;
  CountingValue &operator=(CountingValue &&) = default;

  ~CountingValue() { ++destructions; }

  static void reset() {
    constructions = 0;
    destructions = 0;
  }

  [[nodiscard]] static int live_count() { return constructions - destructions; }
};

TEST(SmallVectorTest, UsesInlineStorageUntilCapacityIsExceeded) {
  SmallVector<int, 2> values;

  const int *const inline_data = values.data();
  EXPECT_TRUE(values.empty());
  EXPECT_EQ(values.size(), 0u);
  EXPECT_EQ(values.capacity(), 2u);
  EXPECT_EQ(values.begin(), values.end());

  values.push_back(3);
  values.push_back(5);

  EXPECT_EQ(values.data(), inline_data);
  EXPECT_EQ(values.capacity(), 2u);
  EXPECT_EQ(values[0], 3);
  EXPECT_EQ(values[1], 5);

  values.push_back(8);

  EXPECT_EQ(values.size(), 3u);
  EXPECT_EQ(values.capacity(), 4u);
  EXPECT_NE(values.data(), inline_data);
  EXPECT_EQ(values[0], 3);
  EXPECT_EQ(values[1], 5);
  EXPECT_EQ(values[2], 8);
}

TEST(SmallVectorTest, ReservePreservesExistingElements) {
  SmallVector<int, 2> values;
  values.push_back(7);
  values.push_back(11);

  const int *const inline_data = values.data();

  values.reserve(8);

  EXPECT_EQ(values.size(), 2u);
  EXPECT_EQ(values.capacity(), 8u);
  EXPECT_NE(values.data(), inline_data);
  EXPECT_EQ(values[0], 7);
  EXPECT_EQ(values[1], 11);

  const int *const reserved_data = values.data();
  values.reserve(4);

  EXPECT_EQ(values.data(), reserved_data);
  EXPECT_EQ(values.capacity(), 8u);
}

TEST(SmallVectorTest, CopyOperationsPreserveValuesWithoutSharingStorage) {
  SmallVector<std::string, 2> source;
  source.emplace_back("alpha");
  source.emplace_back("beta");
  source.emplace_back("gamma");

  SmallVector<std::string, 2> copied(source);

  ASSERT_EQ(copied.size(), 3u);
  EXPECT_NE(copied.data(), source.data());
  EXPECT_EQ(copied[0], "alpha");
  EXPECT_EQ(copied[1], "beta");
  EXPECT_EQ(copied[2], "gamma");

  SmallVector<std::string, 2> assigned;
  assigned.emplace_back("stale");
  assigned = source;

  ASSERT_EQ(assigned.size(), 3u);
  EXPECT_EQ(assigned[0], "alpha");
  EXPECT_EQ(assigned[1], "beta");
  EXPECT_EQ(assigned[2], "gamma");

  source[0] = "changed";
  EXPECT_EQ(copied[0], "alpha");
  EXPECT_EQ(assigned[0], "alpha");
}

TEST(SmallVectorTest, MoveConstructionTransfersHeapStorageAndResetsSource) {
  SmallVector<int, 2> source;
  source.push_back(1);
  source.push_back(2);
  source.push_back(3);

  int *const heap_data = source.data();
  const size_t heap_capacity = source.capacity();

  SmallVector<int, 2> moved(std::move(source));

  ASSERT_EQ(moved.size(), 3u);
  EXPECT_EQ(moved.data(), heap_data);
  EXPECT_EQ(moved.capacity(), heap_capacity);
  EXPECT_EQ(moved[0], 1);
  EXPECT_EQ(moved[1], 2);
  EXPECT_EQ(moved[2], 3);

  EXPECT_TRUE(source.empty());
  EXPECT_EQ(source.capacity(), 2u);
  EXPECT_NE(source.data(), heap_data);
}

TEST(SmallVectorTest, MoveAssignmentReplacesExistingContentsFromInlineSource) {
  SmallVector<std::string, 2> source;
  source.emplace_back("left");
  source.emplace_back("right");

  SmallVector<std::string, 2> destination;
  destination.emplace_back("stale");
  destination.emplace_back("heap");
  destination.emplace_back("value");

  destination = std::move(source);

  ASSERT_EQ(destination.size(), 2u);
  EXPECT_EQ(destination.capacity(), 2u);
  EXPECT_EQ(destination[0], "left");
  EXPECT_EQ(destination[1], "right");

  EXPECT_TRUE(source.empty());
  EXPECT_EQ(source.capacity(), 2u);
}

TEST(SmallVectorTest, AppendCopyAndAppendMoveExtendDestination) {
  SmallVector<int, 2> destination;
  destination.push_back(1);

  SmallVector<int, 2> copied;
  copied.push_back(2);
  copied.push_back(3);

  destination.append_copy(copied);

  ASSERT_EQ(destination.size(), 3u);
  EXPECT_EQ(destination[0], 1);
  EXPECT_EQ(destination[1], 2);
  EXPECT_EQ(destination[2], 3);
  ASSERT_EQ(copied.size(), 2u);
  EXPECT_EQ(copied[0], 2);
  EXPECT_EQ(copied[1], 3);

  SmallVector<int, 2> moved;
  moved.push_back(4);
  moved.push_back(5);

  destination.append_move(moved);

  ASSERT_EQ(destination.size(), 5u);
  EXPECT_EQ(destination[0], 1);
  EXPECT_EQ(destination[1], 2);
  EXPECT_EQ(destination[2], 3);
  EXPECT_EQ(destination[3], 4);
  EXPECT_EQ(destination[4], 5);
  EXPECT_TRUE(moved.empty());
}

TEST(SmallVectorTest, ClearDestroysElementsAndRetainsAllocatedCapacity) {
  CountingValue::reset();

  {
    SmallVector<CountingValue, 2> values;
    values.emplace_back(10);
    values.emplace_back(20);
    values.emplace_back(30);

    ASSERT_EQ(CountingValue::live_count(), 3);
    const size_t grown_capacity = values.capacity();
    EXPECT_GT(grown_capacity, 2u);

    values.clear();

    EXPECT_EQ(values.size(), 0u);
    EXPECT_EQ(values.capacity(), grown_capacity);
    EXPECT_EQ(CountingValue::live_count(), 0);

    values.emplace_back(40);
    EXPECT_EQ(values.size(), 1u);
    EXPECT_EQ(CountingValue::live_count(), 1);
  }

  EXPECT_EQ(CountingValue::live_count(), 0);
}

TEST(BumpVectorTest, EmplaceBackAppendsElements) {
  BumpAllocator allocator(1024);
  BumpVector<int> vector(&allocator);

  vector.emplace_back(10);
  vector.emplace_back(20);
  vector.emplace_back(30);

  EXPECT_EQ(vector.size(), 3u);
  EXPECT_EQ(vector[0], 10);
  EXPECT_EQ(vector[1], 20);
  EXPECT_EQ(vector[2], 30);
}

TEST(BumpVectorTest, PushBackCopyAndMove) {
  BumpAllocator allocator(1024);
  BumpVector<std::string> vector(&allocator);

  std::string value = "hello";
  vector.push_back(value);
  vector.push_back(std::string("world"));

  EXPECT_EQ(vector.size(), 2u);
  EXPECT_EQ(vector[0], "hello");
  EXPECT_EQ(vector[1], "world");
}

TEST(BumpVectorTest, InitialCapacityPreallocates) {
  BumpAllocator allocator(4096);
  BumpVector<int> vector(&allocator, 64);

  EXPECT_EQ(vector.size(), 0u);
  EXPECT_EQ(vector.capacity(), 64u);
}

TEST(BumpVectorTest, GrowsWhenCapacityExceeded) {
  BumpAllocator allocator(4096);
  BumpVector<int> vector(&allocator, 2);

  vector.push_back(1);
  vector.push_back(2);
  vector.push_back(3);

  EXPECT_EQ(vector.size(), 3u);
  EXPECT_GE(vector.capacity(), 3u);
  EXPECT_EQ(vector[0], 1);
  EXPECT_EQ(vector[1], 2);
  EXPECT_EQ(vector[2], 3);
}

TEST(BumpVectorTest, BackReturnsLastElement) {
  BumpAllocator allocator(1024);
  BumpVector<int> vector(&allocator);

  vector.push_back(42);
  vector.push_back(99);

  EXPECT_EQ(vector.back(), 99);
}

TEST(BumpVectorTest, IteratorRangeWorks) {
  BumpAllocator allocator(1024);
  BumpVector<int> vector(&allocator);

  vector.push_back(1);
  vector.push_back(2);
  vector.push_back(3);

  int sum = 0;
  for (int value : vector) {
    sum += value;
  }

  EXPECT_EQ(sum, 6);
}

TEST(BumpVectorTest, EmptyVectorHasZeroSize) {
  BumpAllocator allocator(256);
  BumpVector<int> vector(&allocator);

  EXPECT_TRUE(vector.empty());
  EXPECT_EQ(vector.size(), 0u);
}

TEST(BumpVectorTest, ReserveIncreasesCapacity) {
  BumpAllocator allocator(4096);
  BumpVector<int> vector(&allocator);

  vector.reserve(100);

  EXPECT_GE(vector.capacity(), 100u);
  EXPECT_EQ(vector.size(), 0u);
}

TEST(BumpVectorTest, ReservePreservesExistingElements) {
  BumpAllocator allocator(4096);
  BumpVector<int> vector(&allocator);

  vector.push_back(10);
  vector.push_back(20);
  vector.reserve(64);

  EXPECT_EQ(vector.size(), 2u);
  EXPECT_EQ(vector[0], 10);
  EXPECT_EQ(vector[1], 20);
}

TEST(BumpVectorTest, DataReturnsContiguousStorage) {
  BumpAllocator allocator(1024);
  BumpVector<int> vector(&allocator, 8);

  vector.push_back(1);
  vector.push_back(2);
  vector.push_back(3);

  int *raw = vector.data();
  EXPECT_EQ(raw[0], 1);
  EXPECT_EQ(raw[1], 2);
  EXPECT_EQ(raw[2], 3);
}

TEST(BumpVectorTest, MovableElementsPreservedOnGrow) {
  BumpAllocator allocator(4096);
  BumpVector<std::string> vector(&allocator, 2);

  vector.push_back("alpha");
  vector.push_back("beta");
  vector.push_back("gamma");

  EXPECT_EQ(vector[0], "alpha");
  EXPECT_EQ(vector[1], "beta");
  EXPECT_EQ(vector[2], "gamma");
}

TEST(BumpVectorTest, LargeNumberOfElements) {
  BumpAllocator allocator(64);
  BumpVector<int> vector(&allocator);

  constexpr size_t count = 1000u;
  for (size_t index = 0u; index < count; ++index) {
    vector.push_back(static_cast<int>(index));
  }

  EXPECT_EQ(vector.size(), count);
  for (size_t index = 0u; index < count; ++index) {
    EXPECT_EQ(vector[index], static_cast<int>(index));
  }
}

TEST(BumpVectorTest, MultipleVectorsOnSameAllocator) {
  BumpAllocator allocator(256);
  BumpVector<int> first(&allocator);
  BumpVector<float> second(&allocator);

  first.push_back(1);
  second.push_back(1.0f);
  first.push_back(2);
  second.push_back(2.0f);

  EXPECT_EQ(first[0], 1);
  EXPECT_EQ(first[1], 2);
  EXPECT_FLOAT_EQ(second[0], 1.0f);
  EXPECT_FLOAT_EQ(second[1], 2.0f);
}

TEST(BumpVectorTest, GrowDoesNotCorruptSiblingVectors) {
  BumpAllocator allocator(64);
  BumpVector<int> first(&allocator, 4);
  BumpVector<int> second(&allocator, 4);

  first.push_back(10);
  first.push_back(20);
  second.push_back(30);
  second.push_back(40);

  for (int index = 0; index < 100; ++index) {
    first.push_back(index);
  }

  EXPECT_EQ(second[0], 30);
  EXPECT_EQ(second[1], 40);
  EXPECT_EQ(first[0], 10);
  EXPECT_EQ(first[1], 20);
}

} // namespace
} // namespace astralix
