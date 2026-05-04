#pragma once
#include "arena.hpp"

#include "assert.hpp"

#include <cstddef>
#include <cstring>
#include <string_view>

namespace astralix {

struct StreamBuffer {
public:
  StreamBuffer(size_t capacity)
      : m_arena(capacity > 0 ? capacity : 1u), m_size(capacity) {
    if (capacity > 0) {
      m_data = m_arena.allocate(capacity);
    }
  }

  ~StreamBuffer() {
    m_arena.reset();
    m_data = nullptr;
  }

  void write(char *src, size_t size) {
    auto block = m_arena.allocate(size);

    block->data = src;

    m_data = block;
  }

  void reset() {
    if (m_data != nullptr) {
      m_arena.release(m_data);
      m_data = nullptr;
    }
  }

  char *data() {
    return m_data != nullptr ? static_cast<char *>(m_data->data) : &m_empty;
  }

  size_t size() const { return m_data != nullptr ? m_data->size : m_size; }

private:
  ElasticArena m_arena;
  ElasticArena::Block *m_data = nullptr;
  size_t m_size = 0u;
  char m_empty = '\0';
};

[[nodiscard]] inline Scope<StreamBuffer>
clone_stream_buffer(ElasticArena::Block *block) {
  auto buffer = create_scope<StreamBuffer>(block->size);
  std::memcpy(buffer->data(), block->data, block->size);
  return buffer;
}

[[nodiscard]] inline Scope<StreamBuffer>
stream_buffer_from_string(std::string_view text) {
  auto buffer = create_scope<StreamBuffer>(text.size());
  if (!text.empty()) {
    std::memcpy(buffer->data(), text.data(), text.size());
  }
  return buffer;
}

} // namespace astralix
