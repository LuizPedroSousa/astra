#include "virtual-index-buffer.hpp"

#include <cstring>

namespace astralix {

VirtualIndexBuffer::VirtualIndexBuffer(uint32_t *indices, uint32_t count)
    : m_bytes(count * sizeof(uint32_t)), m_count(count) {
  if (indices != nullptr && !m_bytes.empty()) {
    std::memcpy(m_bytes.data(), indices, m_bytes.size());
  }
}

void VirtualIndexBuffer::bind() const {}

void VirtualIndexBuffer::unbind() const {}

void VirtualIndexBuffer::set_data(const void *data, uint32_t size) const {
  m_bytes.resize(size);
  if (data == nullptr || size == 0) {
    return;
  }

  std::memcpy(m_bytes.data(), data, size);
}

} // namespace astralix
