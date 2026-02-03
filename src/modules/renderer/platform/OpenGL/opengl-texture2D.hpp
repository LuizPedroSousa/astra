#pragma once

#include "guid.hpp"
#include "resources/texture.hpp"

namespace astralix {

class OpenGLTexture2D : public Texture2D {
public:
  OpenGLTexture2D(const ResourceHandle &id, Ref<Texture2DDescriptor> descriptor);

  static Ref<Texture2D> create(const ResourceHandle &resource_id);

  ~OpenGLTexture2D();

  void bind() const override;
  void active(uint32_t slot) const override;

  uint32_t renderer_id() const override { return m_renderer_id; };

  uint32_t width() const override { return m_width; };
  uint32_t height() const override { return m_height; };

private:
  uint32_t m_renderer_id;

  int m_format;
  unsigned char *m_buffer;

  uint32_t m_width;
  uint32_t m_height;

  unsigned int textureParameterToGL(TextureParameter param);
  int textureParameterValueToGL(TextureValue value);
  int formatToGl(TextureFormat format);

  std::unordered_map<TextureParameter, TextureValue> m_parameters;
};
} // namespace astralix
