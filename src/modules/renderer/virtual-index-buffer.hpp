#pragma once

#include "index-buffer.hpp"

#include <cstdint>
#include <vector>

namespace astralix {

class VirtualIndexBuffer : public IndexBuffer {
public:
  VirtualIndexBuffer(uint32_t *indices, uint32_t count);
  ~VirtualIndexBuffer() override = default;

  void bind() const override;
  void unbind() const override;

  uint32_t get_count() const override { return m_count; }
  void set_data(const void *data, uint32_t size) const override;

  const std::vector<uint8_t> &bytes() const noexcept { return m_bytes; }

private:
  mutable std::vector<uint8_t> m_bytes;
  uint32_t m_count = 0;
};

} // namespace astralix
