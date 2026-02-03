#include "opengl-texture2D.hpp"
#include "glad/glad.h"
#include "resources/texture.hpp"

namespace astralix {

OpenGLTexture2D::OpenGLTexture2D(const ResourceHandle &resource_id,
                                 Ref<Texture2DDescriptor> descriptor)
    : Texture2D(resource_id), m_format(formatToGl(descriptor->format)),
      m_parameters(descriptor->parameters) {

  if (descriptor->image_load.has_value()) {
    auto image_load = descriptor->image_load;
    auto image =
        load_image(image_load->path, image_load->flip_image_on_loading);

    m_format = get_image_format(image.nr_channels);

    for (const auto &[param, value] : m_parameters) {
      if (param == TextureParameter::WrapS ||
          param == TextureParameter::WrapT) {

        if (m_format == 4) {
          m_parameters[param] = TextureValue::Repeat;
        } else {
          m_parameters[param] = TextureValue::ClampToEdge;
        }
      }
    }

    m_width = image.width;
    m_height = image.height;

    m_buffer = image.data;
  }

  glGenTextures(1, &m_renderer_id);

  bind();

  if (descriptor->bitmap) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  for (const auto &[param, value] : m_parameters) {
    glTexParameteri(GL_TEXTURE_2D, textureParameterToGL(param),
                    textureParameterValueToGL(value));
  }

  glTexImage2D(GL_TEXTURE_2D, 0, m_format, m_width, m_height, 0, m_format,
               GL_UNSIGNED_BYTE, m_buffer);

  if (m_buffer != nullptr && descriptor->image_load.has_value()) {
    free_image(m_buffer);
  }
}

GLenum OpenGLTexture2D::textureParameterToGL(TextureParameter param) {
  switch (param) {
  case TextureParameter::WrapS:
    return GL_TEXTURE_WRAP_S;
  case TextureParameter::WrapT:
    return GL_TEXTURE_WRAP_T;
  case TextureParameter::MagFilter:
    return GL_TEXTURE_MAG_FILTER;
  case TextureParameter::MinFilter:
    return GL_TEXTURE_MIN_FILTER;
  }
  return 0;
}

GLint OpenGLTexture2D::textureParameterValueToGL(TextureValue value) {
  switch (value) {
  case TextureValue::Repeat:
    return GL_REPEAT;
  case TextureValue::ClampToEdge:
    return GL_CLAMP_TO_EDGE;
  case TextureValue::ClampToBorder:
    return GL_CLAMP_TO_BORDER;
  case TextureValue::LinearMipMap:
    return GL_LINEAR_MIPMAP_LINEAR;
  case TextureValue::Linear:
    return GL_LINEAR;
  case TextureValue::Nearest:
    return GL_NEAREST;
  }
  return 0;
}

int OpenGLTexture2D::formatToGl(TextureFormat format) {
  switch (format) {
  case TextureFormat::Red:
    return GL_RED;

  case TextureFormat::RGB:
    return GL_RGB;

  case TextureFormat::RGBA:
    return GL_RGBA;
  }

  return 0;
}

void OpenGLTexture2D::bind() const {
  glBindTexture(GL_TEXTURE_2D, m_renderer_id);
}

void OpenGLTexture2D::active(uint32_t slot) const {
  glActiveTexture(slot == 0 ? GL_TEXTURE0 : GL_TEXTURE0 + slot);
}

OpenGLTexture2D::~OpenGLTexture2D() { glDeleteTextures(1, &m_renderer_id); }

} // namespace astralix
