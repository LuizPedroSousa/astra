#pragma once

#include "storage-buffer.hpp"
#include <cstdint>
#include <vector>

namespace astralix {

class VirtualStorageBuffer : public StorageBuffer {
public:
  explicit VirtualStorageBuffer(uint32_t size);
  ~VirtualStorageBuffer() override = default;

  void bind() const override;
  void unbind() const override;
  void bind_base(uint32_t point = 0) const override;
  void set_data(const void *data, uint32_t size) const override;
  uint32_t renderer_id() const override { return 0; }

private:
  uint32_t m_size = 0;
  mutable std::vector<uint8_t> m_data;
};

} // namespace astralix
