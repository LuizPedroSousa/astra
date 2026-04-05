#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

namespace astralix {

struct BinnedFreeList {
public:
  static constexpr size_t MIN_BIN_SHIFT = 3u;
  static constexpr size_t MIN_BLOCK_SIZE = 1u << MIN_BIN_SHIFT;
  static constexpr size_t MAX_BINS = 32u;

  BinnedFreeList() = default;
  ~BinnedFreeList() = default;

  BinnedFreeList(const BinnedFreeList &) = delete;
  BinnedFreeList &operator=(const BinnedFreeList &) = delete;

  BinnedFreeList(BinnedFreeList &&other) noexcept { steal_from(other); }

  BinnedFreeList &operator=(BinnedFreeList &&other) noexcept {
    if (this != &other) {
      steal_from(other);
    }
    return *this;
  }

  void *allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
    size_t padded = pad_for_header(size, alignment);
    size_t bin_size = round_up_to_bin(padded);
    size_t bin_index = size_to_bin(bin_size);

    if (bin_index < MAX_BINS) {
      FreeNode *node = m_bins[bin_index];
      if (node != nullptr) {
        m_bins[bin_index] = node->next;
        --m_cached_blocks;
        m_cached_bytes -= bin_size;

        void *raw = static_cast<void *>(node);
        return place_header_and_align(raw, bin_size, alignment);
      }
    }

    void *raw = ::operator new(bin_size);
    ++m_total_allocations;
    m_total_bytes += bin_size;

    return place_header_and_align(raw, bin_size, alignment);
  }

  void deallocate(void *pointer) noexcept {
    if (pointer == nullptr) {
      return;
    }

    BlockHeader *header = reinterpret_cast<BlockHeader *>(
        static_cast<char *>(pointer) - sizeof(BlockHeader)
    );
    size_t bin_size = header->bin_size;
    void *raw = header->raw;
    size_t bin_index = size_to_bin(bin_size);

    if (bin_index < MAX_BINS) {
      FreeNode *node = static_cast<FreeNode *>(raw);
      node->next = m_bins[bin_index];
      m_bins[bin_index] = node;
      ++m_cached_blocks;
      m_cached_bytes += bin_size;
      return;
    }

    ::operator delete(raw);
    --m_total_allocations;
    m_total_bytes -= bin_size;
  }

  template <typename T>
  T *allocate_typed(size_t count = 1u) {
    return static_cast<T *>(allocate(sizeof(T) * count, alignof(T)));
  }

  template <typename T>
  void deallocate_typed(T *pointer) noexcept {
    deallocate(static_cast<void *>(pointer));
  }

  void purge() noexcept {
    for (size_t bin_index = 0u; bin_index < MAX_BINS; ++bin_index) {
      FreeNode *node = m_bins[bin_index];
      while (node != nullptr) {
        FreeNode *next = node->next;
        size_t bin_size = bin_to_size(bin_index);
        ::operator delete(static_cast<void *>(node));
        --m_total_allocations;
        m_total_bytes -= bin_size;
        --m_cached_blocks;
        m_cached_bytes -= bin_size;
        node = next;
      }
      m_bins[bin_index] = nullptr;
    }
  }

  void shrink(size_t max_cached_bytes) noexcept {
    for (size_t bin_index = MAX_BINS; bin_index > 0u && m_cached_bytes > max_cached_bytes;) {
      --bin_index;
      size_t bin_size = bin_to_size(bin_index);
      while (m_bins[bin_index] != nullptr && m_cached_bytes > max_cached_bytes) {
        FreeNode *node = m_bins[bin_index];
        m_bins[bin_index] = node->next;
        ::operator delete(static_cast<void *>(node));
        --m_total_allocations;
        m_total_bytes -= bin_size;
        --m_cached_blocks;
        m_cached_bytes -= bin_size;
      }
    }
  }

  [[nodiscard]] size_t total_allocations() const noexcept { return m_total_allocations; }
  [[nodiscard]] size_t total_bytes() const noexcept { return m_total_bytes; }
  [[nodiscard]] size_t cached_blocks() const noexcept { return m_cached_blocks; }
  [[nodiscard]] size_t cached_bytes() const noexcept { return m_cached_bytes; }

  [[nodiscard]] static size_t round_up_to_bin(size_t size) noexcept {
    if (size <= MIN_BLOCK_SIZE) {
      return MIN_BLOCK_SIZE;
    }
    size_t leading_zeros = __builtin_clzll(size - 1u);
    return 1ull << (64u - leading_zeros);
  }

  [[nodiscard]] static size_t size_to_bin(size_t bin_size) noexcept {
    if (bin_size <= MIN_BLOCK_SIZE) {
      return 0u;
    }
    return __builtin_ctzll(bin_size) - MIN_BIN_SHIFT;
  }

  [[nodiscard]] static size_t bin_to_size(size_t bin_index) noexcept {
    return 1ull << (bin_index + MIN_BIN_SHIFT);
  }

private:
  struct FreeNode {
    FreeNode *next;
  };

  struct BlockHeader {
    void *raw;
    size_t bin_size;
  };

  static size_t pad_for_header(size_t size, size_t alignment) noexcept {
    return sizeof(BlockHeader) + (alignment - 1u) + size;
  }

  static void *place_header_and_align(void *raw, size_t bin_size, size_t alignment) noexcept {
    char *base = static_cast<char *>(raw) + sizeof(BlockHeader);
    uintptr_t address = reinterpret_cast<uintptr_t>(base);
    uintptr_t aligned = (address + alignment - 1u) & ~(alignment - 1u);
    char *result = reinterpret_cast<char *>(aligned);

    BlockHeader *header = reinterpret_cast<BlockHeader *>(result - sizeof(BlockHeader));
    header->raw = raw;
    header->bin_size = bin_size;

    return static_cast<void *>(result);
  }

  void steal_from(BinnedFreeList &other) noexcept {
    for (size_t bin_index = 0u; bin_index < MAX_BINS; ++bin_index) {
      m_bins[bin_index] = other.m_bins[bin_index];
      other.m_bins[bin_index] = nullptr;
    }
    m_total_allocations = other.m_total_allocations;
    m_total_bytes = other.m_total_bytes;
    m_cached_blocks = other.m_cached_blocks;
    m_cached_bytes = other.m_cached_bytes;
    other.m_total_allocations = 0u;
    other.m_total_bytes = 0u;
    other.m_cached_blocks = 0u;
    other.m_cached_bytes = 0u;
  }

  FreeNode *m_bins[MAX_BINS] = {};
  size_t m_total_allocations = 0u;
  size_t m_total_bytes = 0u;
  size_t m_cached_blocks = 0u;
  size_t m_cached_bytes = 0u;
};

} // namespace astralix
