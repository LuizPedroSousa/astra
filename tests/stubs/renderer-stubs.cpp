#include "vertex-array.hpp"
#include "vertex-buffer.hpp"

namespace astralix {

namespace {

class StubVertexBuffer : public VertexBuffer {
public:
  explicit StubVertexBuffer(uint32_t size) : m_size(size) {}
  StubVertexBuffer(const void *, uint32_t size, DrawType) : m_size(size) {}

  void bind() const override {}
  void unbind() const override {}
  void set_data(const void *, uint32_t) override {}
  void set_layout(const BufferLayout &layout) override { m_layout = layout; }
  const BufferLayout &get_layout() const override { return m_layout; }


private:
  uint32_t m_size = 0;
  BufferLayout m_layout;
};

class StubVertexArray : public VertexArray {
public:
  void bind() const override {}
  void unbind() const override {}

  void add_vertex_buffer(const Ref<VertexBuffer> &vertex_buffer) override {
    m_vertex_buffers.push_back(vertex_buffer);
  }

  void set_index_buffer(const Ref<IndexBuffer> &index_buffer) override {
    m_index_buffer = index_buffer;
  }

  const std::vector<Ref<VertexBuffer>> &get_vertex_buffers() const override {
    return m_vertex_buffers;
  }

  const Ref<IndexBuffer> &get_index_buffer() const override {
    return m_index_buffer;
  }

private:
  std::vector<Ref<VertexBuffer>> m_vertex_buffers;
  Ref<IndexBuffer> m_index_buffer = nullptr;
};

} // namespace

Ref<VertexArray> VertexArray::create(RendererBackend) {
  return create_ref<StubVertexArray>();
}

Ref<VertexBuffer> VertexBuffer::create(RendererBackend, uint32_t size) {
  return create_ref<StubVertexBuffer>(size);
}

Ref<VertexBuffer> VertexBuffer::create(RendererBackend, const void *vertices,
                                       uint32_t size, DrawType draw_type) {
  return create_ref<StubVertexBuffer>(vertices, size, draw_type);
}

} // namespace astralix
