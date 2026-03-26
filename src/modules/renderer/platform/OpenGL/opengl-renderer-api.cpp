#include "opengl-renderer-api.hpp"
#include "glad/glad.h"
#include "renderer-api.hpp"
#include <GL/gl.h>
#include <iostream>

namespace astralix {
  void OpenGLRendererAPI::init() { enable_buffer_testing(); }

  void OpenGLRendererAPI::enable_buffer_testing() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
  }

  void OpenGLRendererAPI::disable_buffer_testing() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
  }

  void OpenGLRendererAPI::clear_color(glm::vec4 color) {
    glClearColor(color.x, color.y, color.z, color.w);
  }

  void OpenGLRendererAPI::set_viewport(uint32_t x, uint32_t y, uint32_t width,
    uint32_t height) {
    glViewport(x, y, width, height);
  }

  uint32_t map_clear_buffer_type_to_opengl(ClearBufferType type) {
    uint32_t result = 0;
    uint32_t type_value = static_cast<uint32_t>(type);

    if (type_value & static_cast<uint32_t>(ClearBufferType::Color))
      result |= GL_COLOR_BUFFER_BIT;
    if (type_value & static_cast<uint32_t>(ClearBufferType::Depth))
      result |= GL_DEPTH_BUFFER_BIT;
    if (type_value & static_cast<uint32_t>(ClearBufferType::Stencil))
      result |= GL_STENCIL_BUFFER_BIT;

    return result;
  }

  void OpenGLRendererAPI::clear_buffers(ClearBufferType type) {
    glClear(map_clear_buffer_type_to_opengl(type));
  }

  uint32_t
    OpenGLRendererAPI::map_draw_primitive_type(DrawPrimitive primitive_type) {
    switch (primitive_type) {
    case DrawPrimitive::POINTS:
      return GL_POINTS;
    case DrawPrimitive::LINES:
      return GL_LINES;
    default:
      return GL_TRIANGLES;
    }
  }

  uint32_t OpenGLRendererAPI::map_cull_face_mode(CullFaceMode mode) {
    switch (mode) {
    case CullFaceMode::Back: {
      return GL_BACK;
    }

    case CullFaceMode::Front: {
      return GL_FRONT;
    }

    case CullFaceMode::Left: {
      return GL_LEFT;
    }

    case CullFaceMode::Right: {
      return GL_RIGHT;
    }

    default:
      ASTRA_EXCEPTION("Unsupported OpenGL cull face mode");
    }
  }

  uint32_t OpenGLRendererAPI::map_depth_mode(DepthMode mode) {
    switch (mode) {
    case DepthMode::Less: {
      return GL_LESS;
    }

    case DepthMode::LessEqual: {
      return GL_LEQUAL;
    }

    case DepthMode::Equal: {
      return GL_EQUAL;
    }

    default:
      ASTRA_EXCEPTION("Unsupported OpenGL depth mode");
    }
  }

  void OpenGLRendererAPI::draw_indexed(const Ref<VertexArray>& vertex_array,
    DrawPrimitive primitive_type,
    uint32_t index_count) {
    vertex_array->bind();

    uint32_t count = index_count != -1
      ? index_count
      : vertex_array->get_index_buffer()->get_count();

    glDrawElements(map_draw_primitive_type(primitive_type), count,
      GL_UNSIGNED_INT, 0);

    vertex_array->unbind();
  }

  void OpenGLRendererAPI::cull_face(CullFaceMode mode) {
    glCullFace(map_cull_face_mode(mode));
  }

  void OpenGLRendererAPI::depth(DepthMode mode) {
    glDepthFunc(map_depth_mode(mode));
  }

  void OpenGLRendererAPI::draw_instanced_indexed(DrawPrimitive primitive_type,
    uint32_t index_count,
    uint32_t instance_count) {
    glDrawElementsInstanced(map_draw_primitive_type(primitive_type), index_count,
      GL_UNSIGNED_INT, 0, instance_count);
  }

  void OpenGLRendererAPI::draw_lines(const Ref<VertexArray>& vertex_array,
    uint32_t vertex_count) {

    vertex_array->bind();
    glDrawArrays(GL_LINES, 0, vertex_count);
    vertex_array->unbind();
  }

} // namespace astralix
