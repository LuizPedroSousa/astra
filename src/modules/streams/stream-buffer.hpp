#pragma once
#include "arena.hpp"

#include "assert.hpp"

#include <cstddef>
#include <cstring>

namespace astralix {

struct StreamBuffer {
public:
  StreamBuffer(size_t capacity) : m_arena(capacity) {
    m_data = m_arena.allocate(capacity);
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

  char *data() { return static_cast<char *>(m_data->data); }

  size_t size() const { return m_data->size; }

private:
  ElasticArena m_arena;

  ElasticArena::Block *m_data = nullptr;
  ;
};

[[nodiscard]] inline Scope<StreamBuffer>
clone_stream_buffer(ElasticArena::Block *block) {
  auto buffer = create_scope<StreamBuffer>(block->size);
  std::memcpy(buffer->data(), block->data, block->size);
  return buffer;
}

} // namespace astralix
