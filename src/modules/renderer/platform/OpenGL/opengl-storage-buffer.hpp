#pragma once
#include "storage-buffer.hpp"

namespace astralix {
class OpenGLStorageBuffer : public StorageBuffer {
public:
  OpenGLStorageBuffer(uint32_t size);
  ~OpenGLStorageBuffer();

  void bind() const override;
  void unbind() const override;
  void bind_base(uint32_t point = 0) const override;

  void set_data(const void *data, uint32_t size) const override;
  uint32_t renderer_id() const override { return m_renderer_id; }

private:
  uint32_t m_renderer_id;

  uint32_t m_size;
};

} // namespace astralix
