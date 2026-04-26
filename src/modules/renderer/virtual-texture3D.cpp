#include "virtual-texture3D.hpp"

#include "assert.hpp"
#include "resources/descriptors/texture-descriptor.hpp"

#include <algorithm>
#include <cstring>

namespace astralix {

namespace {

TextureFormat format_from_loaded_channels(int nr_channels) {
  switch (nr_channels) {
  case 1:
    return TextureFormat::Red;
  case 3:
    return TextureFormat::RGB;
  case 4:
    return TextureFormat::RGBA;
  case 2:
  default:
    return TextureFormat::RGBA;
  }
}

std::vector<uint8_t> copy_loaded_image_pixels(const Image &image) {
  const uint32_t width = std::max(image.width, 1);
  const uint32_t height = std::max(image.height, 1);
  const size_t pixel_count =
      static_cast<size_t>(width) * static_cast<size_t>(height);

  if (image.nr_channels == 1 || image.nr_channels == 3 ||
      image.nr_channels == 4) {
    const size_t byte_count =
        pixel_count * static_cast<size_t>(image.nr_channels);
    std::vector<uint8_t> bytes(byte_count, 0);
    if (image.data != nullptr) {
      std::memcpy(bytes.data(), image.data, byte_count);
    }
    return bytes;
  }

  std::vector<uint8_t> bytes(pixel_count * 4u, 0);
  if (image.data == nullptr) {
    return bytes;
  }

  if (image.nr_channels == 2) {
    for (size_t i = 0; i < pixel_count; ++i) {
      const uint8_t luminance = image.data[i * 2u + 0u];
      bytes[i * 4u + 0u] = luminance;
      bytes[i * 4u + 1u] = luminance;
      bytes[i * 4u + 2u] = luminance;
      bytes[i * 4u + 3u] = image.data[i * 2u + 1u];
    }
  }

  return bytes;
}

} // namespace

VirtualTexture3D::VirtualTexture3D(const ResourceHandle &id,
                                   Ref<Texture3DDescriptor> descriptor)
    : Texture3D(id) {
  ASTRA_ENSURE(descriptor->face_paths.size() != 6u,
               "[Vulkan] Cubemap textures require exactly 6 faces");

  m_faces.reserve(descriptor->face_paths.size());

  for (const auto &face_path : descriptor->face_paths) {
    const auto image = load_image(face_path, false);
    const auto face_format = format_from_loaded_channels(image.nr_channels);
    const auto face_width = static_cast<uint32_t>(std::max(image.width, 1));
    const auto face_height = static_cast<uint32_t>(std::max(image.height, 1));

    if (m_faces.empty()) {
      m_format = face_format;
      m_width = face_width;
      m_height = face_height;
    } else {
      ASTRA_ENSURE(face_width != m_width || face_height != m_height,
                   "[Vulkan] Cubemap face dimensions must match");
      ASTRA_ENSURE(face_format != m_format,
                   "[Vulkan] Cubemap face formats must match");
    }

    m_faces.push_back(copy_loaded_image_pixels(image));
    if (image.data != nullptr) {
      free_image(image.data);
    }
  }
}

void VirtualTexture3D::bind() const {}

void VirtualTexture3D::active(uint32_t slot) const { (void)slot; }

} // namespace astralix
