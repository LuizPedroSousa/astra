#pragma once

#include "vertex-array.hpp"

namespace astralix {
class OpenGLVertexArray : public VertexArray {
public:
  OpenGLVertexArray();
  ~OpenGLVertexArray();
  void bind() const override;
  void unbind() const override;

  void add_vertex_buffer(const Ref<VertexBuffer> &vertex_buffer) override;
  void set_index_buffer(const Ref<IndexBuffer> &index_buffer) override;

  const std::vector<Ref<VertexBuffer>> &get_vertex_buffers() const override {
    return m_vertex_buffers;
  };
  const Ref<IndexBuffer> &get_index_buffer() const override {
    return m_index_buffer;
  };

private:
  std::vector<Ref<VertexBuffer>> m_vertex_buffers;
  Ref<IndexBuffer> m_index_buffer;

  uint32_t m_vertex_buffer_index = 0;

  uint32_t m_renderer_id;

  static uint map_shader_data_type_to_opengl(ShaderDataType type);
};
} // namespace astralix
