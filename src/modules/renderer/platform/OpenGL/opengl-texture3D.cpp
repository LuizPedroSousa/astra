#include "opengl-texture3D.hpp"
#include "assert.hpp"
#include "glad/glad.h"
#include "log.hpp"
#include "resources/equirectangular-converter.hpp"

namespace astralix {

OpenGLTexture3D::OpenGLTexture3D(const ResourceHandle &resource_id,
                                 Ref<Texture3DDescriptor> descriptor)
    : Texture3D(resource_id) {
  glGenTextures(1, &m_renderer_id);
  bind();

  if (descriptor->is_from_buffer) {
    m_width = descriptor->buffer_face_width;
    m_height = descriptor->buffer_face_height;

    for (uint32_t face_index = 0; face_index < descriptor->buffer_faces.size(); ++face_index) {
      glTexImage2D(
          GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_index, 0, GL_RGBA,
          m_width, m_height, 0,
          GL_RGBA, GL_UNSIGNED_BYTE, descriptor->buffer_faces[face_index]
      );
    }
  } else if (descriptor->is_equirectangular) {
    auto hdr_image = load_hdr_image(descriptor->equirectangular_path, false);

    LOG_INFO(
        "OpenGLTexture3D: converting equirectangular",
        hdr_image.width, "x", hdr_image.height,
        "to cubemap", descriptor->equirectangular_face_resolution
    );

    auto conversion = convert_equirectangular_to_cubemap(
        hdr_image.data,
        hdr_image.width,
        hdr_image.height,
        hdr_image.nr_channels,
        descriptor->equirectangular_face_resolution
    );

    free_hdr_image(hdr_image.data);

    m_width = descriptor->equirectangular_face_resolution;
    m_height = descriptor->equirectangular_face_resolution;

    for (uint32_t face_index = 0; face_index < 6; ++face_index) {
      glTexImage2D(
          GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_index, 0, GL_RGBA16F,
          m_width, m_height, 0,
          GL_RGBA, GL_FLOAT, conversion.faces[face_index].pixels.data()
      );
    }
  } else if (descriptor->is_from_float_buffer) {
    m_width = descriptor->buffer_face_width;
    m_height = descriptor->buffer_face_height;

    for (uint32_t face_index = 0; face_index < descriptor->float_buffer_faces.size(); ++face_index) {
      glTexImage2D(
          GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_index, 0, GL_RGBA16F,
          m_width, m_height, 0,
          GL_RGBA, GL_FLOAT, descriptor->float_buffer_faces[face_index].data()
      );
    }
  } else {
    for (uint32_t face_index = 0; face_index < descriptor->face_paths.size(); ++face_index) {
      auto image = this->load_image(descriptor->face_paths[face_index], false);
      m_width = image.width;
      m_height = image.height;

      int format = get_image_format(image.nr_channels);
      glTexImage2D(
          GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_index, 0, format, image.width,
          image.height, 0, format, GL_UNSIGNED_BYTE, image.data
      );

      this->free_image(image.data);
    }
  }

  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(
      GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR
  );
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
