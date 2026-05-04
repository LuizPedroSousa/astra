#include "opengl-texture2D.hpp"
#include "glad/glad.h"
#include "resources/texture.hpp"

namespace astralix {

namespace {

void sanitize_parameters(
    std::unordered_map<TextureParameter, TextureValue> &parameters,
    int format
) {
  for (auto &[param, value] : parameters) {
    if (param == TextureParameter::WrapS || param == TextureParameter::WrapT) {
      const bool valid_wrap_value =
          value == TextureValue::Repeat ||
          value == TextureValue::ClampToEdge ||
          value == TextureValue::ClampToBorder;
      if (!valid_wrap_value) {
        value = format == GL_RGBA ? TextureValue::Repeat
                                  : TextureValue::ClampToEdge;
      }
    } else if (param == TextureParameter::MagFilter &&
               value == TextureValue::LinearMipMap) {
      value = TextureValue::Linear;
    }
  }
}

} // namespace

OpenGLTexture2D::OpenGLTexture2D(const ResourceHandle &resource_id, Ref<Texture2DDescriptor> descriptor)
    : Texture2D(resource_id), m_format(formatToGl(descriptor->format)),
      m_buffer(descriptor->buffer), m_width(descriptor->width),
      m_height(descriptor->height), m_parameters(descriptor->parameters) {

  if (descriptor->image_load.has_value()) {
    auto prepared = Texture2D::prepare_descriptor(descriptor);
    m_format = get_image_format(prepared.nr_channels);
    sanitize_parameters(m_parameters, m_format);
    m_width = prepared.width;
    m_height = prepared.height;
    m_buffer = prepared.bytes.empty() ? nullptr : prepared.bytes.data();
  }

  glGenTextures(1, &m_renderer_id);

  bind();

  for (const auto &[param, value] : m_parameters) {
    glTexParameteri(GL_TEXTURE_2D, textureParameterToGL(param), textureParameterValueToGL(value));
  }

  glTexImage2D(GL_TEXTURE_2D, 0, m_format, m_width, m_height, 0, m_format, GL_UNSIGNED_BYTE, m_buffer);

  if (descriptor->bitmap) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  m_buffer = nullptr;
}

OpenGLTexture2D::OpenGLTexture2D(
    const ResourceHandle &resource_id,
    Ref<Texture2DDescriptor> descriptor,
    PreparedTexture2DData prepared
) : Texture2D(resource_id),
    m_format(get_image_format(prepared.nr_channels)),
    m_buffer(prepared.bytes.empty() ? nullptr : prepared.bytes.data()),
    m_width(prepared.width),
    m_height(prepared.height),
    m_parameters(descriptor->parameters) {
  sanitize_parameters(m_parameters, m_format);

  glGenTextures(1, &m_renderer_id);
  bind();

  for (const auto &[param, value] : m_parameters) {
    glTexParameteri(
        GL_TEXTURE_2D,
        textureParameterToGL(param),
        textureParameterValueToGL(value)
    );
  }

  glTexImage2D(
      GL_TEXTURE_2D,
      0,
      m_format,
      m_width,
      m_height,
      0,
      m_format,
      GL_UNSIGNED_BYTE,
      m_buffer
  );

  if (descriptor->bitmap) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  m_buffer = nullptr;
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
    case TextureFormat::RGBA16F:
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
