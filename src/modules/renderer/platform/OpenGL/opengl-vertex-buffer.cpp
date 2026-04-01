#include "opengl-vertex-buffer.hpp"
#include "glad/glad.h"

namespace astralix {

OpenGLVertexBuffer::OpenGLVertexBuffer(const void *vertices, uint32_t size,
                                       DrawType draw_type) {
  glGenBuffers(1, &m_renderer_id);

  bind();

  glBufferData(GL_ARRAY_BUFFER, size, vertices, drawTypeToGL(draw_type));
};

OpenGLVertexBuffer::OpenGLVertexBuffer(uint32_t size) {
  glGenBuffers(1, &m_renderer_id);
  bind();

  glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
};

OpenGLVertexBuffer::~OpenGLVertexBuffer() {
  glDeleteBuffers(1, &m_renderer_id);
};

void OpenGLVertexBuffer::bind() const {
  glBindBuffer(GL_ARRAY_BUFFER, m_renderer_id);
}

void OpenGLVertexBuffer::unbind() const { glBindBuffer(GL_ARRAY_BUFFER, 0); }

void OpenGLVertexBuffer::set_data(const void *data, uint32_t size) {
  bind();
  glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
}

int OpenGLVertexBuffer::drawTypeToGL(DrawType draw_type) {
  switch (draw_type) {
  case DrawType::Dynamic: {
    return GL_DYNAMIC_DRAW;
  }
  case DrawType::Static: {
    return GL_STATIC_DRAW;
  }
  }

  return 0;
}

} // namespace astralix
