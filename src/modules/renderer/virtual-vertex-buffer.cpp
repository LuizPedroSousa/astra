#include "virtual-vertex-buffer.hpp"

#include <cstring>

namespace astralix {

VirtualVertexBuffer::VirtualVertexBuffer(
    const void *vertices, uint32_t size, DrawType draw_type
) {
  (void)draw_type;
  set_data(vertices, size);
}

VirtualVertexBuffer::VirtualVertexBuffer(uint32_t size) : m_bytes(size) {}

void VirtualVertexBuffer::bind() const {}

void VirtualVertexBuffer::unbind() const {}

void VirtualVertexBuffer::set_data(const void *data, uint32_t size) {
  m_bytes.resize(size);
  if (data == nullptr || size == 0) {
    return;
  }

  std::memcpy(m_bytes.data(), data, size);
}

} // namespace astralix
