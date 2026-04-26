#include "opengl-vertex-array.hpp"
#include "assert.hpp"
#include "glad/glad.h"

namespace astralix {
GLenum OpenGLVertexArray::map_shader_data_type_to_opengl(ShaderDataType type) {
  switch (type) {
  case ShaderDataType::Float:
    return GL_FLOAT;
  case ShaderDataType::Float2:
    return GL_FLOAT;
  case ShaderDataType::Float3:
    return GL_FLOAT;
  case ShaderDataType::Float4:
    return GL_FLOAT;
  case ShaderDataType::Mat3:
    return GL_FLOAT;
  case ShaderDataType::Mat4:
    return GL_FLOAT;
  case ShaderDataType::Int:
    return GL_INT;
  case ShaderDataType::Int2:
    return GL_INT;
  case ShaderDataType::Int3:
    return GL_INT;
  case ShaderDataType::Int4:
    return GL_INT;
  case ShaderDataType::Bool:
    return GL_BOOL;
  }

  ASTRA_EXCEPTION("Unknown shader data type");
}

OpenGLVertexArray::OpenGLVertexArray() { glGenVertexArrays(1, &m_renderer_id); }
OpenGLVertexArray::~OpenGLVertexArray() {
  glDeleteVertexArrays(1, &m_renderer_id);
}

void OpenGLVertexArray::bind() const { glBindVertexArray(m_renderer_id); }
void OpenGLVertexArray::unbind() const { glBindVertexArray(0); }

void OpenGLVertexArray::add_vertex_buffer(
    const Ref<VertexBuffer> &vertex_buffer) {

  const auto &layout = vertex_buffer->get_layout();

  ASTRA_ENSURE(layout.get_elements().size() == 0,
                  "Vertex Buffer has no layout elements!")

  bind();

  for (const auto &element : layout.get_elements()) {
    uint32_t location = element.has_explicit_location()
                            ? element.location
                            : m_vertex_buffer_index;

    bool is_matrix = element.type == ShaderDataType::Mat4 ||
                     element.type == ShaderDataType::Mat3;

    if (is_matrix) {
      uint32_t column_count = element.get_component_count();
      uint32_t column_size = sizeof(float) * column_count;

      for (uint32_t column = 0; column < column_count; column++) {
        uint32_t slot = location + column;
        glEnableVertexAttribArray(slot);
        glVertexAttribPointer(
            slot, static_cast<int>(column_count),
            map_shader_data_type_to_opengl(element.type),
            element.normalized ? GL_TRUE : GL_FALSE,
            layout.get_stride(),
            reinterpret_cast<const void *>(element.offset +
                                           column_size * column));
        glVertexAttribDivisor(slot, 1);
      }

      if (!element.has_explicit_location()) {
        m_vertex_buffer_index += column_count;
      }
    } else {
      glEnableVertexAttribArray(location);
      glVertexAttribPointer(
          location, element.get_component_count(),
          map_shader_data_type_to_opengl(element.type),
          element.normalized ? GL_TRUE : GL_FALSE,
          layout.get_stride(),
          reinterpret_cast<const void *>(element.offset));

      if (!element.has_explicit_location()) {
        m_vertex_buffer_index++;
      }
    }
  }
  m_vertex_buffers.push_back(vertex_buffer);
}

void OpenGLVertexArray::set_index_buffer(const Ref<IndexBuffer> &index_buffer) {
  bind();
  index_buffer->bind();

  m_index_buffer = index_buffer;
}

} // namespace astralix
