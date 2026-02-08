#pragma once

#include "assert.hpp"
#include "log.hpp"
#include "trace.hpp"
#include "gtest/gtest.h"
#include <cstddef>
#include <cstring>
#include <list>
#include <stack>

#define KB(N) 1024 * N

namespace astralix {

struct FixedBlockArena {
public:
  FixedBlockArena(size_t block_size, size_t size) : m_block_size(block_size) {
    m_pool = new char[size];
    m_free_blocks.push_back(m_pool);
  }

  ~FixedBlockArena() { delete[] m_pool; }

  void *allocate() {
    ASTRA_ENSURE(m_free_blocks.empty(), "Arena out of memory");

    void *block = m_free_blocks.back();
    m_free_blocks.pop_back();

    m_allocated_memory += m_block_size;

    return block;
  }

  size_t block_size() const { return m_block_size; }

  void release(void *ptr) {
    m_free_blocks.push_back(ptr);
    m_allocated_memory -= m_block_size;
  }

  size_t allocated_memory() { return m_allocated_memory; }

private:
  size_t m_capacity;
  size_t m_block_size;
  size_t m_allocated_memory;

  char *m_pool;

  std::list<void *> m_free_blocks;
};

struct ElasticArena {
public:
  struct BlockPool;

  struct Block {
    BlockPool *pool = nullptr;
    char *data = nullptr;
    size_t size = -1;
  };

  struct BlockPool {
    BlockPool(size_t page_size)
        : page_size(page_size), data(new char[page_size]), allocated_memory(0) {

      for (size_t i = 0; i < page_size; i++) {
        data[i] = 0;
      }

      auto block = new Block{this, data, page_size};

      m_free_blocks.push_back(block);
    }

    void release(Block *block) {
      auto it = m_free_blocks.begin();
      while (it != m_free_blocks.end() && (*it)->data < block->data) {
        ++it;
      }

      auto inserted_it = m_free_blocks.insert(it, block);

      auto left = inserted_it;
      while (left != m_free_blocks.begin()) {
        auto prev = std::prev(left);
        if ((*prev)->data + (*prev)->size == (*left)->data) {
          left = prev;
        } else {
          break;
        }
      }

      // Find the rightmost adjacent block
      auto right = inserted_it;

      while (std::next(right) != m_free_blocks.end()) {
        auto next = std::next(right);
        if ((*right)->data + (*right)->size == (*next)->data) {
          right = next; // Move right if adjacent
        } else {
          break;
        }
      }

      //  Merge all blocks from left to right into a single block
      char *start = (*left)->data;
      char *end = (*right)->data + (*right)->size;

      size_t total_size = end - start;

      Block *merged_block = new Block{this, start, total_size};

      // Remove the old blocks from left to right (inclusive)
      auto remove_start = left;
      auto remove_end = std::next(right);

      for (auto i = remove_start; i != remove_end; ++i) {
        *i = 0;
      }

      auto new_it = m_free_blocks.erase(remove_start, remove_end);

      m_free_blocks.insert(new_it, merged_block);

      allocated_memory -= block->size;
    }

    bool is_full(size_t block_size) {
      return m_free_blocks.empty() || allocated_memory + block_size > page_size;
    }

    Block *allocate(size_t block_size) {
      Block *block = m_free_blocks.front();

      ASTRA_ENSURE(block->size < block_size, "No block large enough");

      m_free_blocks.pop_front();

      if (block->size > block_size) {
        char *remaining_start = block->data + block_size;

        size_t remaining_size = block->size - block_size;
        m_free_blocks.push_front(
            new Block{this, remaining_start, remaining_size});
      }

      block->size = block_size;

      allocated_memory += block->size;

      return block;
    }

    std::list<Block *> m_free_blocks;

    ~BlockPool() {
      for (Block *block : m_free_blocks) {
        delete block;
      }

      m_free_blocks.clear();

      delete[] data;
    }

    size_t page_size;
    char *data;
    size_t allocated_memory;
  };

  ElasticArena(size_t page_size, size_t max_capacity_increase = 12)
      : m_capacity(page_size), m_initial_page_size(page_size),
        m_max_capacity_increase(max_capacity_increase), m_allocated_memory(0),
        m_current_capacity_increased(0) {
    if (page_size == 0) {
      ASTRA_EXCEPTION("page_size must be greater than 0");
    }

    m_pools.push_back(new BlockPool(page_size));
  }

  ElasticArena() = default;

  ~ElasticArena() {
    for (auto pool : m_pools) {
      delete pool;
    }
  }

  Block *allocate(size_t block_size) {
    for (auto pool : m_pools) {
      if (pool->is_full(block_size))
        continue;

      auto block = pool->allocate(block_size);

      m_allocated_memory += block_size;

      return block;
    }

    ASTRA_ENSURE(m_current_capacity_increased >= m_max_capacity_increase,
                 "Arena out of capacity increase");

    size_t fixed_capacity = std::max(m_initial_page_size, block_size * 2);

    auto pool = new BlockPool(fixed_capacity);

    m_pools.push_back(pool);

    m_capacity += fixed_capacity;
    m_current_capacity_increased += 1;

    auto block = pool->allocate(block_size);

    m_allocated_memory += block_size;

    return block;
  }

  void release(Block *block) {
    for (auto pool : m_pools) {
      if (block->pool == pool) {
        pool->release(block);
        m_allocated_memory -= block->size;
        return;
      }
    }

    ASTRA_EXCEPTION("Invalid pointer to release");
  }

  size_t page_size() const { return m_initial_page_size; }

  void reset() {
    for (BlockPool *pool : m_pools) {
      delete pool;
    }

    m_pools.clear();
    m_pools.push_back(new BlockPool(m_initial_page_size));

    m_allocated_memory = 0;
    m_capacity = m_initial_page_size;
  }

  size_t pool_size() { return m_pools.size(); }
  size_t allocated_memory() const { return m_allocated_memory; }

  size_t capacity_increase_count() const {
    return m_current_capacity_increased;
  }

  size_t max_capacity_increase() const { return m_max_capacity_increase; }

  size_t capacity() const { return m_capacity; }

private:
  size_t m_capacity = 0;
  size_t m_initial_page_size = 0;
  size_t m_allocated_memory = 0;

  std::list<BlockPool *> m_pools;

  size_t m_max_capacity_increase = 12;
  size_t m_current_capacity_increased = 0;
};

struct StackArena {
public:
  StackArena(FixedBlockArena &arena) : m_arena(arena), m_position(nullptr) {}

  void *push() {
    void *block = m_arena.allocate();

    m_stack.push(block);

    m_position = block;

    return block;
  }

  size_t allocated_memory() { return m_stack.size() * m_arena.block_size(); }

  size_t curr_memory_allocated() {
    if (m_position == nullptr)
      return 0;

    return m_arena.block_size();
  }

  void pop() {
    if (m_stack.empty())
      return;

    m_position = m_stack.top();

    m_stack.pop();

    m_arena.release(m_position);
  }

  ~StackArena() {
    if (!m_stack.empty())
      clear();
  }

  void clear() {
    while (!m_stack.empty()) {
      pop();
    }
  }

private:
  FixedBlockArena &m_arena;

  void *m_position;

  std::stack<void *> m_stack;
};

struct ElasticStackArena {
public:
  ElasticStackArena(ElasticArena &arena)
      : m_arena(arena), m_position(nullptr) {}

  ElasticArena::Block *push(size_t block_size) {
    ElasticArena::Block *block = m_arena.allocate(block_size);

    m_stack.push(block);

    m_position = block;

    return block;
  }

  size_t allocated_memory() { return m_arena.allocated_memory(); }

  size_t curr_memory_allocated() { return m_position->size; }

  void pop() {
    auto block = m_stack.top();

    m_arena.release(block);

    m_stack.pop();

    m_position = m_stack.size() > 0 ? m_stack.top() : nullptr;
  }

  void release() {
    if (m_stack.empty())
      return;

    while (!m_stack.empty()) {

      auto block = m_stack.top();

      block->pool->release(block);
    }
  }

  void clear() {
    while (!m_stack.empty()) {
      release();
    }
  }

private:
  ElasticArena &m_arena;

  ElasticArena::Block *m_position;

  std::stack<ElasticArena::Block *> m_stack;
};

template <size_t N> struct S_Str {
public:
  S_Str(const char (&str)[N]) : m_arena(sizeof(char) * N) {
    block = m_arena.allocate(N);

    char *dest = block->data;

    for (size_t i = 0; i < N; ++i) {
      dest[i] = ensure_char_padding(str[i]);
    }
  }

  template <size_t M> S_Str &operator=(const char (&str)[M]) {
    if (block != nullptr) {
      m_arena.release(block);
    }

    block = m_arena.allocate(M);

    char *dest = block->data;

    for (size_t i = 0; i < M; ++i) {
      dest[i] = ensure_char_padding(str[i]);
    }

    return *this;
  }

  const char *c_str() { return block->data; }

  size_t size() { return m_arena.allocated_memory(); }

private:
  ElasticArena m_arena;

  constexpr char ensure_char_padding(const char c) { return !c ? '\0' : c; }

  ElasticArena::Block *block = nullptr;
};

template <size_t N> S_Str(const char (&)[N]) -> S_Str<N>;

struct Str {
public:
  struct StrInitializer {
    ElasticArena &arena;
    std::variant<const char *, const std::string *> str;

    const char *get_c_str() const {
      if (std::holds_alternative<const char *>(str)) {
        return std::get<const char *>(str);
      } else {
        return std::get<const std::string *>(str)->c_str();
      }
    }
  };

  static constexpr size_t SSO_CAPACITY = 22;

  Str(const char *str, ElasticArena &arena);

  Str(ElasticArena &arena) : m_arena(arena), m_size(0) { m_sso[0] = '\0'; };

  Str &append(Str &other, size_t size) {
    size_t new_size = m_size + size;

    if (m_is_sso || !m_size && size <= SSO_CAPACITY) {
      if (new_size > SSO_CAPACITY) {
        auto block = convert_sso_to_arena(new_size);

        auto src = other.is_sso() ? other.m_sso : other.m_block->data;

        std::memcpy(block->data + m_size, src, size);

        m_size = new_size;

        return *this;
      }

      if (other.is_sso()) {
        for (size_t i = 0; i < size; i++) {
          m_sso[m_size + i] = other.m_sso[i];
        }
      }

      m_size += size;
      m_is_sso = true;

      return *this;
    }

    auto src = other.is_sso() ? other.m_sso : other.m_block->data;

    m_block = merge_block(src, new_size, size);
    m_size = new_size;

    return *this;
  }

  Str &append(const char *str, size_t size) {
    size_t new_size = m_size + size;

    if (m_is_sso || !m_size && size <= SSO_CAPACITY) {
      if (new_size > SSO_CAPACITY) {
        auto block = convert_sso_to_arena(new_size);

        std::strcpy(block->data, str);

        m_size = new_size;

        return *this;
      }

      for (size_t i = 0; i < size; i++) {
        m_sso[m_size + i] = str[i];
      }

      m_size += size;
      m_is_sso = true;

      return *this;
    }

    m_block = merge_block(str, new_size, size);
    m_size = new_size;

    return *this;
  }

  Str &append(size_t value, size_t size) {
    char bytes[4];

    for (size_t i = 0; i < size; ++i) {
      bytes[i] = static_cast<char>((value >> ((size - 1 - i) * 4)) & 0xFF);
    }

    return append(bytes, size);
  }

  inline Str &append(Str &other) {
    append(other, other.size());

    return *this;
  }

  inline Str &operator+=(const Str &other) {
    append(other, other.size());

    return *this;
  }

  inline Str &operator+=(const char *other) {
    if (!m_size) {
      *this = other;
      return *this;
    }

    *this += Str(other, m_arena);

    return *this;
  }

  inline Str &operator=(const std::string &str) {
    *this = str.c_str();

    return *this;
  }

  inline Str &operator=(std::string str) {
    *this = str.c_str();

    return *this;
  }

  inline Str &operator=(const char *str) {
    if (this->c_str() == str) {
      return *this;
    }

    release();

    new (this) Str(str, m_arena);

    return *this;
  }

  inline Str &operator=(const StrInitializer &initializer) {
    const char *cstr = initializer.get_c_str();
    if (this->c_str() == cstr) {
      return *this;
    }
    release();
    if (std::holds_alternative<const char *>(initializer.str)) {
      // Use the provided arena for const char*
      new (this) Str(cstr, initializer.arena);
    } else {
      // Use the existing m_arena for const std::string*
      new (this) Str(cstr, m_arena);
    }
    return *this;
  }

  bool operator==(std::string other) const {
    if (m_is_sso) {
      return other.size() == m_size &&
             memcmp(m_sso, other.c_str(), m_size) == 0;
    }

    if (m_block != nullptr) {
      return memcmp(m_block->data, other.c_str(), m_size) == 0;
    }

    return false;
  }

  bool operator==(const char *other) const {
    if (m_is_sso) {
      return strcmp(m_sso, other) == 0;
    }

    if (m_block != nullptr) {
      return strcmp(m_block->data, other) == 0;
    }

    return false;
  }

  static Str from_initializer(const StrInitializer &init) {
    return Str(init.get_c_str(), init.arena);
  }

  inline const char *c_str() const {
    if (m_is_sso) {
      return m_sso;
    }

    return m_block != nullptr ? m_block->data : nullptr;
  }

  operator const char *() const { return c_str(); }
  operator std::string() const { return c_str(); }

  bool is_sso() { return m_is_sso; }

  size_t size() const { return m_size; }

  size_t allocated_memory();

  ~Str();

  void release();

private:
  inline ElasticArena::Block *convert_sso_to_arena(size_t size) {
    auto block = m_arena.allocate(size);

    auto dest = block->data;

    for (size_t i = 0; i < m_size; i++) {
      dest[i] = m_sso[i];
      m_sso[i] = 0;
    }

    m_is_sso = false;

    m_block = block;

    return block;
  }

  inline ElasticArena::Block *merge_block(const char *src, size_t new_size,
                                          size_t size) {
    size_t page_size = std::max(allocated_memory() * 2, new_size);

    auto new_block = m_arena.allocate(page_size + 1);

    new_block->data[new_size] = '\0';

    if (m_block != nullptr) {
      std::memcpy(new_block->data, m_block->data, m_size);
      std::memcpy(new_block->data + m_size, src, size);
      m_arena.release(m_block);
    } else {
      std::memcpy(new_block->data, src, size);
    }

    return new_block;
  }

  ElasticArena &m_arena;
  bool m_is_self_arena = false;
  bool m_is_sso = false;
  char m_sso[SSO_CAPACITY + 1];
  size_t m_size = 0;

  ElasticArena::Block *m_block = nullptr;
};

// template <size_t N> Str(const char (&)[N]) -> Str<N>;

template <typename T> class Vector {
public:
  Vector(size_t page_size) : m_arena(page_size) {}

  void push_back(const T &element) {
    auto block = m_arena.allocate(sizeof(T));
    m_data[0] = block;
  }

  size_t size() { return m_arena.allocated_memory(); }

private:
  ElasticArena::Block **m_data = nullptr;

  ElasticArena m_arena;
};

} // namespace astralix
