#pragma once

#include "resources/texture.hpp"

#include <cstdint>
#include <vector>

namespace astralix {

class VirtualTexture3D : public Texture3D {
public:
  VirtualTexture3D(const ResourceHandle &id, Ref<Texture3DDescriptor> descriptor);
  ~VirtualTexture3D() = default;

  void bind() const override;
  void active(uint32_t slot) const override;
  uint32_t renderer_id() const override { return 0; }
  uint32_t width() const override { return m_width; }
  uint32_t height() const override { return m_height; }

  TextureFormat format() const noexcept { return m_format; }
  bool is_hdr() const noexcept { return m_is_hdr; }
  uint32_t face_count() const noexcept {
    return static_cast<uint32_t>(m_faces.size());
  }
  const std::vector<std::vector<uint8_t>> &faces() const noexcept {
    return m_faces;
  }
  const std::vector<std::vector<float>> &hdr_faces() const noexcept {
    return m_hdr_faces;
  }

private:
  TextureFormat m_format = TextureFormat::RGBA;
  bool m_is_hdr = false;
  uint32_t m_width = 1;
  uint32_t m_height = 1;
  std::vector<std::vector<uint8_t>> m_faces;
  std::vector<std::vector<float>> m_hdr_faces;
};

} // namespace astralix
