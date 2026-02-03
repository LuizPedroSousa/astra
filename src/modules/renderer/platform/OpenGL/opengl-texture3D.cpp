#include "opengl-texture3D.hpp"
#include "assert.hpp"
#include "glad/glad.h"

namespace astralix {

OpenGLTexture3D::OpenGLTexture3D(const ResourceHandle &resource_id,
                                 Ref<Texture3DDescriptor> descriptor)
    : Texture3D(resource_id) {
  glGenTextures(1, &m_renderer_id);
  bind();

  for (u_int i = 0; i < descriptor->face_paths.size(); i++) {
    auto image = this->load_image(descriptor->face_paths[i], false);
    m_width += image.width;
    m_height += image.height;

    int format = get_image_format(image.nr_channels);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, image.width,
                 image.height, 0, format, GL_UNSIGNED_BYTE, image.data);

    this->free_image(image.data);
  }

  // set the texture wrapping/filtering options (on currently bound cube_map)
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

OpenGLTexture3D::~OpenGLTexture3D() { glDeleteTextures(1, &m_renderer_id); }

void OpenGLTexture3D::active(uint32_t slot) const {
  glActiveTexture(slot == 0 ? GL_TEXTURE0 : GL_TEXTURE0 + slot);
}

void OpenGLTexture3D::bind() const {
  glBindTexture(GL_TEXTURE_CUBE_MAP, m_renderer_id);
}

} // namespace astralix
