#include "virtual-texture2D.hpp"

#include "resources/descriptors/texture-descriptor.hpp"

#include <algorithm>
#include <cstring>

namespace astralix {

namespace {

uint32_t bytes_per_pixel(TextureFormat format) {
  switch (format) {
  case TextureFormat::Red:
    return 1;
  case TextureFormat::RGB:
    return 3;
  case TextureFormat::RGBA:
  default:
    return 4;
  }
}

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

void flip_rows_in_place(std::vector<uint8_t> &bytes, uint32_t width,
                        uint32_t height, TextureFormat format) {
  if (height <= 1 || width == 0 || bytes.empty()) {
    return;
  }

  const size_t row_stride = static_cast<size_t>(width) *
                            static_cast<size_t>(bytes_per_pixel(format));
  if (row_stride == 0 || bytes.size() < row_stride * static_cast<size_t>(height)) {
    return;
  }

  std::vector<uint8_t> scratch(row_stride, 0);
  for (uint32_t y = 0; y < height / 2; ++y) {
    const size_t top_offset = static_cast<size_t>(y) * row_stride;
    const size_t bottom_offset =
        static_cast<size_t>(height - 1u - y) * row_stride;

    std::memcpy(scratch.data(), bytes.data() + top_offset, row_stride);
    std::memcpy(bytes.data() + top_offset, bytes.data() + bottom_offset,
                row_stride);
    std::memcpy(bytes.data() + bottom_offset, scratch.data(), row_stride);
  }
}

std::vector<uint8_t> copy_descriptor_buffer(Ref<Texture2DDescriptor> descriptor,
                                            uint32_t width, uint32_t height) {
  const size_t byte_count =
      static_cast<size_t>(width) * static_cast<size_t>(height) *
      bytes_per_pixel(descriptor->format);
  std::vector<uint8_t> bytes(byte_count, 0);
  if (descriptor->buffer != nullptr && byte_count > 0) {
    std::memcpy(bytes.data(), descriptor->buffer, byte_count);
  }
  return bytes;
}

} // namespace

VirtualTexture2D::VirtualTexture2D(const ResourceHandle &id,
                                   Ref<Texture2DDescriptor> descriptor)
    : Texture2D(id) {
  if (descriptor->image_load.has_value()) {
    auto prepared = Texture2D::prepare_descriptor(descriptor);
    m_format = format_from_loaded_channels(prepared.nr_channels);
    m_width = prepared.width;
    m_height = prepared.height;
    m_bytes.assign(prepared.bytes.begin(), prepared.bytes.end());
    flip_rows_in_place(m_bytes, m_width, m_height, m_format);
    return;
  }

  m_format = descriptor->format;
  m_width = std::max(descriptor->width, 1u);
  m_height = std::max(descriptor->height, 1u);
  m_bytes = copy_descriptor_buffer(descriptor, m_width, m_height);
}

VirtualTexture2D::VirtualTexture2D(
    const ResourceHandle &id,
    Ref<Texture2DDescriptor> descriptor,
    PreparedTexture2DData prepared
) : Texture2D(id) {
  (void)descriptor;
  m_format = format_from_loaded_channels(prepared.nr_channels);
  m_width = std::max(prepared.width, 1u);
  m_height = std::max(prepared.height, 1u);
  m_bytes.assign(prepared.bytes.begin(), prepared.bytes.end());
  flip_rows_in_place(m_bytes, m_width, m_height, m_format);
}

void VirtualTexture2D::bind() const {}

void VirtualTexture2D::active(uint32_t slot) const { (void)slot; }

} // namespace astralix
