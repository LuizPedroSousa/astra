#pragma once

#include "log.hpp"
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
#include <vector>

namespace astralix {

struct BumpAllocator {
public:
  explicit BumpAllocator(size_t capacity)
      : m_base(capacity > 0u ? static_cast<char *>(::operator new(capacity)) : nullptr),
        m_capacity(capacity), m_offset(0u) {}

  ~BumpAllocator() {
    for (auto &chunk : m_retired_chunks) {
      ::operator delete(chunk.base);
    }
    ::operator delete(m_base);
  }

  BumpAllocator(const BumpAllocator &) = delete;
  BumpAllocator &operator=(const BumpAllocator &) = delete;

  BumpAllocator(BumpAllocator &&other) noexcept
      : m_base(other.m_base), m_capacity(other.m_capacity),
        m_offset(other.m_offset),
        m_retired_chunks(std::move(other.m_retired_chunks)) {
    other.m_base = nullptr;
    other.m_capacity = 0u;
    other.m_offset = 0u;
  }

  BumpAllocator &operator=(BumpAllocator &&other) noexcept {
    if (this != &other) {
      for (auto &chunk : m_retired_chunks) {
        ::operator delete(chunk.base);
      }
      ::operator delete(m_base);
      m_base = other.m_base;
      m_capacity = other.m_capacity;
      m_offset = other.m_offset;
      m_retired_chunks = std::move(other.m_retired_chunks);
      other.m_base = nullptr;
      other.m_capacity = 0u;
      other.m_offset = 0u;
    }
    return *this;
  }

  void *allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
    size_t aligned_offset = align_up_from_base(m_offset, alignment);
    size_t next_offset = aligned_offset + size;

    if (next_offset > m_capacity) {
      retire_and_grow(size, alignment);
      aligned_offset = align_up_from_base(m_offset, alignment);
      next_offset = aligned_offset + size;
    }

    m_offset = next_offset;
    return m_base + aligned_offset;
  }

  template <typename T, typename... Args>
  T *allocate_object(Args &&...arguments) {
    void *storage = allocate(sizeof(T), alignof(T));
    return ::new (storage) T(std::forward<Args>(arguments)...);
  }

  template <typename T>
  T *allocate_array(size_t count) {
    if (count == 0u) {
      return nullptr;
    }
    void *storage = allocate(sizeof(T) * count, alignof(T));
    T *array = static_cast<T *>(storage);
    for (size_t index = 0u; index < count; ++index) {
      ::new (array + index) T();
    }
    return array;
  }

  void reset() {
    for (auto &chunk : m_retired_chunks) {
      ::operator delete(chunk.base);
    }
    m_retired_chunks.clear();
    m_offset = 0u;
  }

  size_t offset() const { return m_offset; }
  size_t capacity() const { return m_capacity; }
  size_t remaining() const { return m_capacity - m_offset; }
  size_t chunk_count() const { return m_retired_chunks.size() + 1u; }

  void reserve(size_t requested_capacity) {
    if (requested_capacity <= m_capacity) {
      return;
    }
    if (m_offset == 0u) {
      ::operator delete(m_base);
      m_base = static_cast<char *>(::operator new(requested_capacity));
      m_capacity = requested_capacity;
    } else {
      retire_and_grow(requested_capacity, 1u);
    }
  }

private:
  struct RetiredChunk {
    char *base;
    size_t capacity;
  };

  static size_t align_up(size_t value, size_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
  }

  size_t align_up_from_base(size_t offset, size_t alignment) const {
    uintptr_t address = reinterpret_cast<uintptr_t>(m_base) + offset;
    uintptr_t aligned_address = (address + alignment - 1u) & ~(alignment - 1u);
    return offset + (aligned_address - address);
  }

  void retire_and_grow(size_t allocation_size, size_t alignment) {
    const size_t padded_need = allocation_size + alignment;

    size_t next_capacity = m_capacity + m_capacity / 2u;

    if (next_capacity < padded_need) {
      next_capacity = padded_need;
    }

    if (m_base != nullptr) {
      m_retired_chunks.push_back({m_base, m_capacity});
    }

    m_base = static_cast<char *>(::operator new(next_capacity));
    m_capacity = next_capacity;
    m_offset = 0u;
    LOG_DEBUG("reserved");
  }

  char *m_base = nullptr;
  size_t m_capacity = 0u;
  size_t m_offset = 0u;
  std::vector<RetiredChunk> m_retired_chunks;
};
} // namespace astralix
