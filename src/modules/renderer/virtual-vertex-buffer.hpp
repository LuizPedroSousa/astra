#pragma once

#include "vertex-buffer.hpp"

#include <cstdint>
#include <vector>

namespace astralix {

class VirtualVertexBuffer : public VertexBuffer {
public:
  VirtualVertexBuffer(const void *vertices, uint32_t size, DrawType draw_type);
  explicit VirtualVertexBuffer(uint32_t size);
  ~VirtualVertexBuffer() override = default;

  void bind() const override;
  void unbind() const override;

  void set_data(const void *data, uint32_t size) override;

  const BufferLayout &get_layout() const override { return m_layout; }
  void set_layout(const BufferLayout &layout) override { m_layout = layout; }

  const std::vector<uint8_t> &bytes() const noexcept { return m_bytes; }

private:
  std::vector<uint8_t> m_bytes;
  BufferLayout m_layout;
};

} // namespace astralix
