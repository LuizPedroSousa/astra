#include "resources/texture.hpp"

namespace astralix {

class OpenGLTexture3D : public Texture3D {
public:
  OpenGLTexture3D(const ResourceHandle &resource_id,
                  Ref<Texture3DDescriptor> descriptor);

  ~OpenGLTexture3D();

  void bind() const override;
  void active(uint32_t slot) const override;
  uint32_t renderer_id() const override { return m_renderer_id; };
  uint32_t width() const override { return m_width; };
  uint32_t height() const override { return m_height; };

private:
  uint32_t m_width;
  uint32_t m_height;
  uint32_t m_renderer_id;
};

} // namespace astralix
