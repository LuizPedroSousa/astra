#include "bump.hpp"
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

using namespace astralix;

TEST(BumpAllocatorTest, AllocateReturnsNonNullPointer) {
  BumpAllocator allocator(1024);

  void *pointer = allocator.allocate(64);

  ASSERT_NE(pointer, nullptr);
  EXPECT_EQ(allocator.offset(), 64u);
}

TEST(BumpAllocatorTest, SequentialAllocationsAreContiguous) {
  BumpAllocator allocator(1024);

  auto *first = static_cast<char *>(allocator.allocate(16, 1));
  auto *second = static_cast<char *>(allocator.allocate(16, 1));

  EXPECT_EQ(second, first + 16);
}

TEST(BumpAllocatorTest, AllocationsRespectAlignment) {
  BumpAllocator allocator(1024);

  allocator.allocate(1, 1);

  void *aligned = allocator.allocate(8, 16);

  EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned) % 16, 0u);
}

TEST(BumpAllocatorTest, ResetRestoresOffsetToZero) {
  BumpAllocator allocator(256);

  allocator.allocate(128);
  EXPECT_EQ(allocator.offset(), 128u);

  allocator.reset();
  EXPECT_EQ(allocator.offset(), 0u);
}

TEST(BumpAllocatorTest, ResetAllowsReuse) {
  BumpAllocator allocator(256);

  void *before = allocator.allocate(64, 1);
  allocator.reset();
  void *after = allocator.allocate(64, 1);

  EXPECT_EQ(before, after);
}

TEST(BumpAllocatorTest, RetiresChunkWhenCapacityExceeded) {
  BumpAllocator allocator(64);

  allocator.allocate(32, 1);
  EXPECT_EQ(allocator.chunk_count(), 1u);

  allocator.allocate(128, 1);
  EXPECT_EQ(allocator.chunk_count(), 2u);
}

TEST(BumpAllocatorTest, OldPointersRemainValidAfterGrow) {
  BumpAllocator allocator(32);

  auto *first = static_cast<char *>(allocator.allocate(16, 1));
  std::memset(first, 0xAB, 16);

  allocator.allocate(64, 1);

  for (size_t index = 0u; index < 16u; ++index) {
    EXPECT_EQ(static_cast<unsigned char>(first[index]), 0xAB)
        << "byte at index " << index
        << " was corrupted after allocator grew";
  }
}

TEST(BumpAllocatorTest, ReserveIncreasesCapacityWithoutChangingOffset) {
  BumpAllocator allocator(64);

  allocator.reserve(512);

  EXPECT_GE(allocator.capacity(), 512u);
  EXPECT_EQ(allocator.offset(), 0u);
}

TEST(BumpAllocatorTest, ReserveIsNoOpWhenAlreadySufficient) {
  BumpAllocator allocator(256);

  const size_t original_capacity = allocator.capacity();
  allocator.reserve(128);

  EXPECT_EQ(allocator.capacity(), original_capacity);
}

TEST(BumpAllocatorTest, RemainingReportsAvailableBytes) {
  BumpAllocator allocator(256);

  EXPECT_EQ(allocator.remaining(), 256u);

  allocator.allocate(100, 1);
  EXPECT_EQ(allocator.remaining(), 156u);
}

TEST(BumpAllocatorTest, ZeroCapacityAllocatorGrowsOnFirstAllocate) {
  BumpAllocator allocator(0);

  EXPECT_EQ(allocator.capacity(), 0u);

  void *pointer = allocator.allocate(32);

  ASSERT_NE(pointer, nullptr);
  EXPECT_GE(allocator.capacity(), 32u);
}

TEST(BumpAllocatorTest, AllocateObjectConstructsInPlace) {
  BumpAllocator allocator(256);

  struct Payload {
    int x;
    float y;
    Payload(int x, float y) : x(x), y(y) {}
  };

  auto *payload = allocator.allocate_object<Payload>(42, 3.14f);

  ASSERT_NE(payload, nullptr);
  EXPECT_EQ(payload->x, 42);
  EXPECT_FLOAT_EQ(payload->y, 3.14f);
}

TEST(BumpAllocatorTest, AllocateObjectRespectsAlignment) {
  BumpAllocator allocator(256);

  struct alignas(32) AlignedPayload {
    uint64_t value;
  };

  allocator.allocate(1, 1);
  auto *payload = allocator.allocate_object<AlignedPayload>();

  EXPECT_EQ(reinterpret_cast<uintptr_t>(payload) % 32, 0u);
}

TEST(BumpAllocatorTest, AllocateArrayDefaultConstructsElements) {
  BumpAllocator allocator(1024);

  struct Element {
    int value = 99;
  };

  auto *array = allocator.allocate_array<Element>(4);

  ASSERT_NE(array, nullptr);
  for (size_t index = 0u; index < 4u; ++index) {
    EXPECT_EQ(array[index].value, 99)
        << "element at index " << index << " was not default constructed";
  }
}

TEST(BumpAllocatorTest, AllocateArrayWithZeroCountReturnsNull) {
  BumpAllocator allocator(256);

  int *array = allocator.allocate_array<int>(0);

  EXPECT_EQ(array, nullptr);
  EXPECT_EQ(allocator.offset(), 0u);
}

TEST(BumpAllocatorTest, MoveConstructorTransfersOwnership) {
  BumpAllocator original(256);
  original.allocate(64);

  BumpAllocator moved(std::move(original));

  EXPECT_EQ(moved.capacity(), 256u);
  EXPECT_EQ(moved.offset(), 64u);
  EXPECT_EQ(original.capacity(), 0u);
  EXPECT_EQ(original.offset(), 0u);
}

TEST(BumpAllocatorTest, MoveAssignmentTransfersOwnership) {
  BumpAllocator first(256);
  first.allocate(64);

  BumpAllocator second(128);
  second = std::move(first);

  EXPECT_EQ(second.capacity(), 256u);
  EXPECT_EQ(second.offset(), 64u);
  EXPECT_EQ(first.capacity(), 0u);
}

TEST(BumpAllocatorTest, ResetFreesRetiredChunks) {
  BumpAllocator allocator(32);

  allocator.allocate(16, 1);
  allocator.allocate(64, 1);
  EXPECT_EQ(allocator.chunk_count(), 2u);

  allocator.reset();
  EXPECT_EQ(allocator.chunk_count(), 1u);
  EXPECT_EQ(allocator.offset(), 0u);
}

TEST(BumpAllocatorTest, MultipleGrowsRetireMultipleChunks) {
  BumpAllocator allocator(16);

  for (int iteration = 0; iteration < 5; ++iteration) {
    allocator.allocate(32, 1);
  }

  EXPECT_GT(allocator.chunk_count(), 1u);
}
