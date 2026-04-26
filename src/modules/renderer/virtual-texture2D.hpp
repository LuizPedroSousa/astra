#pragma once

#include "resources/texture.hpp"

#include <cstdint>
#include <vector>

namespace astralix {

class VirtualTexture2D : public Texture2D {
public:
  VirtualTexture2D(const ResourceHandle &id, Ref<Texture2DDescriptor> descriptor);
  ~VirtualTexture2D() = default;

  void bind() const override;
  void active(uint32_t slot) const override;
  uint32_t renderer_id() const override { return 0; }
  uint32_t width() const override { return m_width; }
  uint32_t height() const override { return m_height; }

  TextureFormat format() const noexcept { return m_format; }
  const std::vector<uint8_t> &bytes() const noexcept { return m_bytes; }

private:
  TextureFormat m_format = TextureFormat::RGBA;
  uint32_t m_width = 1;
  uint32_t m_height = 1;
  std::vector<uint8_t> m_bytes;
};

} // namespace astralix
