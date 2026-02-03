#pragma once

#include "base.hpp"

namespace astralix {

enum class RendererBackend;

class IndexBuffer {
public:
  virtual ~IndexBuffer() = default;

  virtual void bind() const = 0;
  virtual void unbind() const = 0;

  virtual uint32_t get_count() const = 0;

  virtual void set_data(const void *data, uint32_t size) const = 0;

  static Ref<IndexBuffer> create(RendererBackend backend, u_int32_t *indices,
                                 u_int32_t count);
};

} // namespace astralix
