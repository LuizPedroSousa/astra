#include "opengl-framebuffer.hpp"
#include "assert.hpp"
#include "either.hpp"
#include "engine.hpp"
#include "framebuffer.hpp"
#include <GL/gl.h>
#include <GL/glext.h>

namespace astralix {

static const uint32_t s_max_framebuffer_size = 8192;

namespace utils {

static GLenum get_texture_target(bool multisampled) {
  return multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
}

static void create_textures(uint32_t *outID, uint32_t count) {
  glGenTextures(count, outID);
}

static void bind_texture(bool multisampled, uint32_t id) {
  glBindTexture(get_texture_target(multisampled), id);
}

static void attach_color_texture(uint32_t id, int samples,
                                 GLenum internal_format, GLenum format,
                                 uint32_t width, uint32_t height, int index) {
  bool multisampled = samples > 1;
  if (multisampled) {
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internal_format,
                            width, height, GL_TRUE);
  } else {
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format,
                 GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_LINEAR);
  }

  switch (internal_format) {
  case GL_DEPTH_COMPONENT:
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           id, 0);
    break;
  default:
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index,
                           get_texture_target(multisampled), id, 0);
  }
}

static void attach_depth(uint32_t id, int samples, GLenum format,
                         GLenum attachment_type, uint32_t width,
                         uint32_t height) {

  glGenRenderbuffers(1, &id);
  glBindRenderbuffer(GL_RENDERBUFFER, id);

  bool is_multisampled = samples > 1;

  if (is_multisampled) {
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, width,
                                     height);
  } else {
    glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
  }

  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment_type, GL_RENDERBUFFER,
                            id);
}

static bool is_depth_format(FramebufferTextureFormat format) {
  switch (format) {
  case FramebufferTextureFormat::DEPTH24STENCIL8:
    return true;
  }

  return false;
}

uint32_t static map_bind_type_to_opengl(FramebufferBindType type) {
  switch (type) {
  case FramebufferBindType::Read:
    return GL_READ_FRAMEBUFFER;

  case FramebufferBindType::Draw:
    return GL_DRAW_FRAMEBUFFER;
  default:
    return GL_FRAMEBUFFER;
  }
};

}; // namespace utils

OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification &spec)
    : m_specification(spec) {
  for (auto spec : m_specification.attachments.attachments) {
    if (!utils::is_depth_format(spec.format))
      m_color_attachment_specifications.emplace_back(spec);
    else
      m_depth_attachment_specification = spec;
  }

  invalidate();
}

OpenGLFramebuffer::~OpenGLFramebuffer() {
  glDeleteFramebuffers(1, &m_renderer_id);
  glDeleteTextures(m_color_attachments.size(), m_color_attachments.data());
  glDeleteTextures(1, &m_depth_attachment);
};

void OpenGLFramebuffer::bind(FramebufferBindType type, uint32_t id) {
  glBindFramebuffer(utils::map_bind_type_to_opengl(type),
                    id != -1 ? id : m_renderer_id);
  glViewport(0, 0, m_specification.width, m_specification.height);
};

void OpenGLFramebuffer::unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); };

void OpenGLFramebuffer::resize(uint32_t width, uint32_t height) {

  if (width == 0 || height == 0 || width > s_max_framebuffer_size ||
      height > s_max_framebuffer_size) {
    return;
  }
  m_specification.width = width;
  m_specification.height = height;

  invalidate();
}

int OpenGLFramebuffer::read_pixel(uint32_t attachment_index, int x, int y) {
  ASTRA_ENSURE(attachment_index > m_color_attachments.size(),
               "Invalid attachment index");

  glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment_index);

  int pixel_data;
  glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixel_data);

  return pixel_data;
}

void OpenGLFramebuffer::clear_attachment(uint32_t attachment_index, int value) {
  auto &spec = m_color_attachment_specifications[attachment_index];

  // Retrieve the texture ID
  GLuint textureID = m_color_attachments[attachment_index];

  // Bind the texture
  glBindTexture(GL_TEXTURE_2D, textureID);

  // Get the texture dimensions
  GLsizei width, height;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

  // Create an array with the specified color value
  int *data = new int[width * height];
  for (int i = 0; i < width * height; ++i) {
    data[i] = value;
  }

  // Clear the texture data
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_INT, data);

  // Unbind the texture
  glBindTexture(GL_TEXTURE_2D, 0);

  // Delete the temporary data array
  delete[] data;
}

void OpenGLFramebuffer::invalidate() {
  if (m_renderer_id != 0) {
    glDeleteFramebuffers(1, &m_renderer_id);
    glDeleteTextures(m_color_attachments.size(), m_color_attachments.data());
    glDeleteTextures(1, &m_depth_attachment);

    m_color_attachments.clear();
    m_depth_attachment = 0;
  }

  glGenFramebuffers(1, &m_renderer_id);
  bind();

  bool multisample = m_specification.samples > 1;

  // Attachments
  if (m_color_attachment_specifications.size()) {
    m_color_attachments.resize(m_color_attachment_specifications.size());
    utils::create_textures(m_color_attachments.data(),
                           m_color_attachments.size());

    for (size_t i = 0; i < m_color_attachments.size(); i++) {
      utils::bind_texture(multisample, m_color_attachments[i]);

      switch (m_color_attachment_specifications[i].format) {
      case FramebufferTextureFormat::RGBA8:
        utils::attach_color_texture(
            m_color_attachments[i], m_specification.samples, GL_RGBA8, GL_RGBA,
            m_specification.width, m_specification.height, i);
        break;
      case FramebufferTextureFormat::RGBA16F:
        utils::attach_color_texture(
            m_color_attachments[i], m_specification.samples, GL_RGBA16F,
            GL_RGBA, m_specification.width, m_specification.height, i);
        break;
      case FramebufferTextureFormat::RGBA32F:
        utils::attach_color_texture(
            m_color_attachments[i], m_specification.samples, GL_RGBA32F,
            GL_RGBA, m_specification.width, m_specification.height, i);
        break;
      case FramebufferTextureFormat::RED_INTEGER:
        utils::attach_color_texture(
            m_color_attachments[i], m_specification.samples, GL_R32I,
            GL_RED_INTEGER, m_specification.width, m_specification.height, i);
        break;

      case FramebufferTextureFormat::None:
      case FramebufferTextureFormat::DEPTH24STENCIL8:
        break;
      case FramebufferTextureFormat::DEPTH_ONLY:
        utils::attach_color_texture(m_color_attachments[i],
                                    m_specification.samples, GL_DEPTH_COMPONENT,
                                    GL_DEPTH_COMPONENT, m_specification.width,
                                    m_specification.height, i);
        break;
      }
    }
  }

  if (m_depth_attachment_specification.format !=
      FramebufferTextureFormat::None) {
    utils::create_textures(&m_depth_attachment, 1);
    utils::bind_texture(multisample, m_depth_attachment);
    switch (m_depth_attachment_specification.format) {
    case FramebufferTextureFormat::DEPTH24STENCIL8:
      utils::attach_depth(m_depth_attachment, m_specification.samples,
                          GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT,
                          m_specification.width, m_specification.height);
      break;

    default:
      break;
    }
  }

  if (m_color_attachments.size() > 1) {
    ASTRA_ENSURE(m_color_attachments.size() > 4, "invalid color attachments");

    auto is_depth_only = false;

    for (auto color_attachment : m_color_attachment_specifications) {
      if (color_attachment.format == FramebufferTextureFormat::DEPTH_ONLY) {
        is_depth_only = true;
        break;
      }
    }

    if (is_depth_only) {
      glDrawBuffer(GL_NONE);
      glReadBuffer(GL_NONE);
    } else {
      GLenum buffers[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                           GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};

      glDrawBuffers(m_color_attachments.size(), buffers);
    }

  } else if (m_color_attachments.empty()) {
    // Only depth-pass
    glDrawBuffer(GL_NONE);
  }

  ASTRA_ENSURE(glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
                   GL_FRAMEBUFFER_COMPLETE,
               "Can't create framebuffer");

  unbind();
}

void OpenGLFramebuffer::blit(uint32_t width, uint32_t height) {
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

} // namespace astralix
