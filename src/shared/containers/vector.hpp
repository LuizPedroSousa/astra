#pragma once

#include "binned-free-list.hpp"
#include "bump.hpp"
#include <algorithm>
#include <cstddef>
#include <type_traits>

namespace astralix {

inline BinnedFreeList &default_binned_free_list() {
  thread_local BinnedFreeList instance;
  return instance;
}

template <typename T, size_t InlineCapacity>
class SmallVector {
  static_assert(InlineCapacity > 0u);

public:
  SmallVector() noexcept : m_allocator(&default_binned_free_list()) {}

  explicit SmallVector(BinnedFreeList *allocator) noexcept
      : m_allocator(allocator) {}

  SmallVector(const SmallVector &other)
      : m_allocator(other.m_allocator) {
    append_copy(other);
  }

  SmallVector(BinnedFreeList *allocator, const SmallVector &other)
      : m_allocator(allocator) {
    append_copy(other);
  }

  SmallVector(SmallVector &&other) noexcept(
      std::is_nothrow_move_constructible_v<T>
  )
      : m_allocator(other.m_allocator) {
    move_from(std::move(other));
  }

  ~SmallVector() {
    destroy_elements();
    release_heap_storage();
  }

  SmallVector &operator=(const SmallVector &other) {
    if (this == &other) {
      return *this;
    }

    clear();
    append_copy(other);
    return *this;
  }

  SmallVector &operator=(SmallVector &&other) noexcept(
      std::is_nothrow_move_constructible_v<T>
  ) {
    if (this == &other) {
      return *this;
    }

    destroy_elements();
    release_heap_storage();
    m_data = inline_data();
    m_capacity = InlineCapacity;
    m_size = 0u;
    m_allocator = other.m_allocator;
    move_from(std::move(other));
    return *this;
  }

  void release() noexcept {
    destroy_elements();
    release_heap_storage();
    m_data = inline_data();
    m_size = 0u;
    m_capacity = InlineCapacity;
  }

  [[nodiscard]] bool empty() const noexcept { return m_size == 0u; }
  [[nodiscard]] size_t size() const noexcept { return m_size; }
  [[nodiscard]] size_t capacity() const noexcept { return m_capacity; }

  [[nodiscard]] T *data() noexcept { return m_data; }
  [[nodiscard]] const T *data() const noexcept { return m_data; }

  [[nodiscard]] T *begin() noexcept { return m_data; }
  [[nodiscard]] const T *begin() const noexcept { return m_data; }
  [[nodiscard]] T *end() noexcept { return m_data + m_size; }
  [[nodiscard]] const T *end() const noexcept { return m_data + m_size; }

  [[nodiscard]] T &operator[](size_t index) noexcept { return m_data[index]; }
  [[nodiscard]] const T &operator[](size_t index) const noexcept {
    return m_data[index];
  }

  void clear() noexcept {
    destroy_elements();
    m_size = 0u;
  }

  void reserve(size_t min_capacity) {
    if (min_capacity <= m_capacity) {
      return;
    }

    T *next_data = allocate(min_capacity);
    for (size_t index = 0u; index < m_size; ++index) {
      std::construct_at(
          next_data + index, std::move_if_noexcept(m_data[index])
      );
    }

    destroy_elements();
    release_heap_storage();
    m_data = next_data;
    m_capacity = min_capacity;
  }

  template <typename... Args>
  T &emplace_back(Args &&...args) {
    if (m_size == m_capacity) {
      reserve(next_capacity(m_size + 1u));
    }

    T *slot = m_data + m_size;
    std::construct_at(slot, std::forward<Args>(args)...);
    ++m_size;
    return *slot;
  }

  void push_back(const T &value) { emplace_back(value); }
  void push_back(T &&value) { emplace_back(std::move(value)); }

  void append_copy(const SmallVector &other) {
    reserve(m_size + other.m_size);
    for (const auto &value : other) {
      emplace_back(value);
    }
  }

  void append_move(SmallVector &other) {
    reserve(m_size + other.m_size);
    for (auto &value : other) {
      emplace_back(std::move(value));
    }
    other.clear();
  }

private:
  using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;

  [[nodiscard]] bool using_inline_storage() const noexcept {
    return m_data == inline_data();
  }

  [[nodiscard]] T *inline_data() noexcept {
    return std::launder(reinterpret_cast<T *>(m_inline_storage));
  }

  [[nodiscard]] const T *inline_data() const noexcept {
    return std::launder(reinterpret_cast<const T *>(m_inline_storage));
  }

  [[nodiscard]] T *allocate(size_t count) {
    return m_allocator->allocate_typed<T>(count);
  }

  void deallocate(T *pointer) noexcept {
    m_allocator->deallocate_typed(pointer);
  }

  void destroy_elements() noexcept {
    for (size_t index = m_size; index > 0u; --index) {
      std::destroy_at(m_data + (index - 1u));
    }
  }

  void release_heap_storage() noexcept {
    if (!using_inline_storage()) {
      deallocate(m_data);
    }
  }

  void move_from(SmallVector &&other) {
    if (!other.using_inline_storage()) {
      m_data = other.m_data;
      m_size = other.m_size;
      m_capacity = other.m_capacity;
      other.m_data = other.inline_data();
      other.m_size = 0u;
      other.m_capacity = InlineCapacity;
      return;
    }

    reserve(other.m_size);
    for (auto &value : other) {
      emplace_back(std::move(value));
    }
    other.clear();
  }

  [[nodiscard]] size_t next_capacity(size_t min_capacity) const noexcept {
    size_t next = m_capacity > 0u ? m_capacity : InlineCapacity;
    while (next < min_capacity) {
      next *= 2u;
    }
    return next;
  }

  BinnedFreeList *m_allocator;
  Storage m_inline_storage[InlineCapacity];
  T *m_data = inline_data();
  size_t m_size = 0u;
  size_t m_capacity = InlineCapacity;
};

template <typename T>
struct BumpVector {
public:
  BumpVector() = default;

  explicit BumpVector(BumpAllocator *allocator)
      : m_allocator(allocator), m_data(nullptr), m_size(0u), m_capacity(0u) {}

  BumpVector(BumpAllocator *allocator, size_t initial_capacity)
      : m_allocator(allocator), m_size(0u) {
    if (initial_capacity > 0u) {
      m_data = m_allocator->allocate_array<T>(initial_capacity);
      m_capacity = initial_capacity;
    } else {
      m_data = nullptr;
      m_capacity = 0u;
    }
  }

  size_t size() const { return m_size; }
  size_t capacity() const { return m_capacity; }
  bool empty() const { return m_size == 0u; }
  T *data() { return m_data; }
  const T *data() const { return m_data; }

  T &operator[](size_t index) { return m_data[index]; }
  const T &operator[](size_t index) const { return m_data[index]; }

  T &back() { return m_data[m_size - 1u]; }
  const T &back() const { return m_data[m_size - 1u]; }

  T *begin() { return m_data; }
  T *end() { return m_data + m_size; }
  const T *begin() const { return m_data; }
  const T *end() const { return m_data + m_size; }

  template <typename... Args>
  T &emplace_back(Args &&...arguments) {
    ensure_capacity(m_size + 1u);
    T *slot = m_data + m_size;
    ::new (slot) T(std::forward<Args>(arguments)...);
    ++m_size;
    return *slot;
  }

  void push_back(const T &value) { emplace_back(value); }
  void push_back(T &&value) { emplace_back(std::move(value)); }

  void reserve(size_t requested_capacity) {
    ensure_capacity(requested_capacity);
  }

private:
  void ensure_capacity(size_t required) {
    if (required <= m_capacity) {
      return;
    }

    size_t next_capacity = m_capacity + m_capacity / 2u;
    if (next_capacity < required) {
      next_capacity = required;
    }

    T *next_data = static_cast<T *>(
        m_allocator->allocate(sizeof(T) * next_capacity, alignof(T))
    );

    for (size_t index = 0u; index < m_size; ++index) {
      ::new (next_data + index) T(std::move(m_data[index]));
    }

    m_data = next_data;
    m_capacity = next_capacity;
  }

  BumpAllocator *m_allocator = nullptr;
  T *m_data = nullptr;
  size_t m_size = 0u;
  size_t m_capacity = 0u;
};

} // namespace astralix
