#include "virtual-vertex-array.hpp"

namespace astralix {

void VirtualVertexArray::bind() const {}

void VirtualVertexArray::unbind() const {}

void VirtualVertexArray::add_vertex_buffer(
    const Ref<VertexBuffer> &vertex_buffer
) {
  m_vertex_buffers.push_back(vertex_buffer);
}

void VirtualVertexArray::set_index_buffer(
    const Ref<IndexBuffer> &index_buffer
) {
  m_index_buffer = index_buffer;
}

} // namespace astralix
