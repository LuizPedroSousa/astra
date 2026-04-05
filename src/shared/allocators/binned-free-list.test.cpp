#include "binned-free-list.hpp"
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

using namespace astralix;

TEST(BinnedFreeListTest, RoundUpToBinReturnsMinBlockForSmallSizes) {
  EXPECT_EQ(BinnedFreeList::round_up_to_bin(1u), 8u);
  EXPECT_EQ(BinnedFreeList::round_up_to_bin(5u), 8u);
  EXPECT_EQ(BinnedFreeList::round_up_to_bin(8u), 8u);
}

TEST(BinnedFreeListTest, RoundUpToBinReturnsPowerOfTwo) {
  EXPECT_EQ(BinnedFreeList::round_up_to_bin(9u), 16u);
  EXPECT_EQ(BinnedFreeList::round_up_to_bin(16u), 16u);
  EXPECT_EQ(BinnedFreeList::round_up_to_bin(17u), 32u);
  EXPECT_EQ(BinnedFreeList::round_up_to_bin(100u), 128u);
  EXPECT_EQ(BinnedFreeList::round_up_to_bin(1024u), 1024u);
  EXPECT_EQ(BinnedFreeList::round_up_to_bin(1025u), 2048u);
}

TEST(BinnedFreeListTest, BinIndexRoundTrips) {
  for (size_t shift = BinnedFreeList::MIN_BIN_SHIFT; shift < 20u; ++shift) {
    size_t bin_size = 1ull << shift;
    size_t bin_index = BinnedFreeList::size_to_bin(bin_size);
    EXPECT_EQ(BinnedFreeList::bin_to_size(bin_index), bin_size)
        << "round-trip failed for bin_size=" << bin_size;
  }
}

TEST(BinnedFreeListTest, AllocateReturnsNonNull) {
  BinnedFreeList allocator;

  void *pointer = allocator.allocate(64);
  ASSERT_NE(pointer, nullptr);
  EXPECT_EQ(allocator.total_allocations(), 1u);

  allocator.deallocate(pointer);
  allocator.purge();
}

TEST(BinnedFreeListTest, AllocateRespectsAlignment) {
  BinnedFreeList allocator;

  void *aligned_8 = allocator.allocate(32, 8);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned_8) % 8, 0u);

  void *aligned_16 = allocator.allocate(32, 16);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned_16) % 16, 0u);

  void *aligned_32 = allocator.allocate(32, 32);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned_32) % 32, 0u);

  void *aligned_64 = allocator.allocate(32, 64);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned_64) % 64, 0u);

  allocator.deallocate(aligned_8);
  allocator.deallocate(aligned_16);
  allocator.deallocate(aligned_32);
  allocator.deallocate(aligned_64);
  allocator.purge();
}

TEST(BinnedFreeListTest, DeallocatedBlockIsReused) {
  BinnedFreeList allocator;

  void *first = allocator.allocate(64);
  allocator.deallocate(first);

  EXPECT_EQ(allocator.cached_blocks(), 1u);

  void *second = allocator.allocate(64);

  EXPECT_EQ(allocator.cached_blocks(), 0u);
  EXPECT_EQ(allocator.total_allocations(), 1u);

  allocator.deallocate(second);
  allocator.purge();
}

TEST(BinnedFreeListTest, DifferentSizesUseDifferentBins) {
  BinnedFreeList allocator;

  void *small = allocator.allocate(8);
  void *large = allocator.allocate(256);

  allocator.deallocate(small);
  allocator.deallocate(large);

  EXPECT_EQ(allocator.cached_blocks(), 2u);

  void *reused_small = allocator.allocate(8);
  EXPECT_EQ(allocator.cached_blocks(), 1u);

  void *reused_large = allocator.allocate(256);
  EXPECT_EQ(allocator.cached_blocks(), 0u);

  allocator.deallocate(reused_small);
  allocator.deallocate(reused_large);
  allocator.purge();
}

TEST(BinnedFreeListTest, WrittenDataSurvivesRoundTrip) {
  BinnedFreeList allocator;

  auto *data = static_cast<char *>(allocator.allocate(128));
  std::memset(data, 0xAB, 128);

  for (size_t index = 0u; index < 128u; ++index) {
    EXPECT_EQ(static_cast<unsigned char>(data[index]), 0xAB)
        << "byte at index " << index << " was corrupted";
  }

  allocator.deallocate(data);
  allocator.purge();
}

TEST(BinnedFreeListTest, AllocateTypedReturnsAlignedPointer) {
  BinnedFreeList allocator;

  struct alignas(16) Payload {
    float values[4];
  };

  Payload *payload = allocator.allocate_typed<Payload>();
  ASSERT_NE(payload, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(payload) % 16, 0u);

  payload->values[0] = 1.0f;
  payload->values[3] = 4.0f;
  EXPECT_FLOAT_EQ(payload->values[0], 1.0f);
  EXPECT_FLOAT_EQ(payload->values[3], 4.0f);

  allocator.deallocate_typed(payload);
  allocator.purge();
}

TEST(BinnedFreeListTest, AllocateTypedArrayReturnsCorrectCount) {
  BinnedFreeList allocator;

  int *array = allocator.allocate_typed<int>(16);
  ASSERT_NE(array, nullptr);

  for (size_t index = 0u; index < 16u; ++index) {
    array[index] = static_cast<int>(index * 10);
  }

  for (size_t index = 0u; index < 16u; ++index) {
    EXPECT_EQ(array[index], static_cast<int>(index * 10));
  }

  allocator.deallocate_typed(array);
  allocator.purge();
}

TEST(BinnedFreeListTest, PurgeFreesCachedBlocks) {
  BinnedFreeList allocator;

  void *first = allocator.allocate(64);
  void *second = allocator.allocate(128);
  allocator.deallocate(first);
  allocator.deallocate(second);

  EXPECT_EQ(allocator.cached_blocks(), 2u);
  EXPECT_GT(allocator.cached_bytes(), 0u);

  allocator.purge();

  EXPECT_EQ(allocator.cached_blocks(), 0u);
  EXPECT_EQ(allocator.cached_bytes(), 0u);
  EXPECT_EQ(allocator.total_allocations(), 0u);
  EXPECT_EQ(allocator.total_bytes(), 0u);
}

TEST(BinnedFreeListTest, ShrinkReducesCacheToThreshold) {
  BinnedFreeList allocator;

  std::vector<void *> pointers;
  for (int iteration = 0; iteration < 10; ++iteration) {
    pointers.push_back(allocator.allocate(1024));
  }
  for (auto *pointer : pointers) {
    allocator.deallocate(pointer);
  }

  size_t cached_before = allocator.cached_bytes();
  EXPECT_GT(cached_before, 1024u);

  allocator.shrink(1024u);

  EXPECT_LE(allocator.cached_bytes(), 1024u);
  EXPECT_LT(allocator.cached_blocks(), 10u);

  allocator.purge();
}

TEST(BinnedFreeListTest, DeallocateNullIsNoOp) {
  BinnedFreeList allocator;

  allocator.deallocate(nullptr);

  EXPECT_EQ(allocator.cached_blocks(), 0u);
  EXPECT_EQ(allocator.total_allocations(), 0u);
}

TEST(BinnedFreeListTest, MultipleAllocationsOfSameSizeReuseFromSameBin) {
  BinnedFreeList allocator;

  void *first = allocator.allocate(32);
  void *second = allocator.allocate(32);
  void *third = allocator.allocate(32);

  allocator.deallocate(first);
  allocator.deallocate(second);
  allocator.deallocate(third);

  EXPECT_EQ(allocator.cached_blocks(), 3u);

  void *reused_a = allocator.allocate(32);
  void *reused_b = allocator.allocate(32);
  void *reused_c = allocator.allocate(32);

  EXPECT_EQ(allocator.cached_blocks(), 0u);
  EXPECT_EQ(allocator.total_allocations(), 3u);

  allocator.deallocate(reused_a);
  allocator.deallocate(reused_b);
  allocator.deallocate(reused_c);
  allocator.purge();
}

TEST(BinnedFreeListTest, MoveConstructorTransfersOwnership) {
  BinnedFreeList original;

  void *pointer = original.allocate(64);
  original.deallocate(pointer);
  EXPECT_EQ(original.cached_blocks(), 1u);

  BinnedFreeList moved(std::move(original));

  EXPECT_EQ(moved.cached_blocks(), 1u);
  EXPECT_EQ(moved.total_allocations(), 1u);
  EXPECT_EQ(original.cached_blocks(), 0u);
  EXPECT_EQ(original.total_allocations(), 0u);

  moved.purge();
}

TEST(BinnedFreeListTest, MoveAssignmentTransfersOwnership) {
  BinnedFreeList first;
  void *pointer = first.allocate(64);
  first.deallocate(pointer);

  BinnedFreeList second;
  second = std::move(first);

  EXPECT_EQ(second.cached_blocks(), 1u);
  EXPECT_EQ(first.cached_blocks(), 0u);

  second.purge();
}

TEST(BinnedFreeListTest, GeometricGrowthPatternReusesOldBuffers) {
  BinnedFreeList allocator;

  void *buffer = allocator.allocate(64);
  allocator.deallocate(buffer);

  buffer = allocator.allocate(256);
  allocator.deallocate(buffer);

  buffer = allocator.allocate(1024);
  allocator.deallocate(buffer);

  buffer = allocator.allocate(4096);
  allocator.deallocate(buffer);

  EXPECT_EQ(allocator.cached_blocks(), 4u);

  void *reused_64 = allocator.allocate(64);
  void *reused_1024 = allocator.allocate(1024);

  EXPECT_EQ(allocator.cached_blocks(), 2u);

  allocator.deallocate(reused_64);
  allocator.deallocate(reused_1024);

  allocator.purge();
}

TEST(BinnedFreeListTest, SimulatesSmallVectorGrowthPattern) {
  BinnedFreeList allocator;

  constexpr size_t initial_capacity = 4u;
  size_t capacity = initial_capacity;
  int *buffer = allocator.allocate_typed<int>(capacity);

  for (size_t index = 0u; index < capacity; ++index) {
    buffer[index] = static_cast<int>(index);
  }

  for (int growth_step = 0; growth_step < 5; ++growth_step) {
    size_t next_capacity = capacity * 2u;
    int *next_buffer = allocator.allocate_typed<int>(next_capacity);

    for (size_t index = 0u; index < capacity; ++index) {
      next_buffer[index] = buffer[index];
    }

    allocator.deallocate_typed(buffer);

    for (size_t index = capacity; index < next_capacity; ++index) {
      next_buffer[index] = static_cast<int>(index);
    }

    buffer = next_buffer;
    capacity = next_capacity;
  }

  for (size_t index = 0u; index < capacity; ++index) {
    EXPECT_EQ(buffer[index], static_cast<int>(index))
        << "data corrupted at index " << index;
  }

  EXPECT_GT(allocator.cached_blocks(), 0u);

  allocator.deallocate_typed(buffer);
  allocator.purge();
}
