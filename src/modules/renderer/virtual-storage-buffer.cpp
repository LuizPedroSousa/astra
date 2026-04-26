#include "virtual-storage-buffer.hpp"
#include <algorithm>
#include <cstring>

namespace astralix {

VirtualStorageBuffer::VirtualStorageBuffer(uint32_t size)
    : m_size(size), m_data(size) {}

void VirtualStorageBuffer::bind() const {}

void VirtualStorageBuffer::unbind() const {}

void VirtualStorageBuffer::bind_base(uint32_t point) const { (void)point; }

void VirtualStorageBuffer::set_data(const void *data, uint32_t size) const {
  if (data == nullptr || size == 0) {
    return;
  }

  const auto copy_size = std::min(size, m_size);
  std::memcpy(m_data.data(), data, copy_size);
}

} // namespace astralix
