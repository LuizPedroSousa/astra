#include "equirectangular-converter.hpp"

#include <cmath>
#include <algorithm>

namespace astralix {

namespace {

constexpr float PI = 3.14159265358979323846f;

struct Vec3 {
  float x, y, z;
};

Vec3 normalize(Vec3 vector) {
  float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
  return {vector.x / length, vector.y / length, vector.z / length};
}

Vec3 cube_face_direction(uint32_t face, float u, float v) {
  float mapped_u = 2.0f * u - 1.0f;
  float mapped_v = 2.0f * v - 1.0f;

  switch (face) {
  case 0: return { 1.0f, -mapped_v, -mapped_u};
  case 1: return {-1.0f, -mapped_v,  mapped_u};
  case 2: return { mapped_u,  1.0f,  mapped_v};
  case 3: return { mapped_u, -1.0f, -mapped_v};
  case 4: return { mapped_u, -mapped_v,  1.0f};
  case 5: return {-mapped_u, -mapped_v, -1.0f};
  default: return {0.0f, 0.0f, 0.0f};
  }
}

void sample_equirectangular_bilinear(
    const float *pixels,
    int source_width,
    int source_height,
    int source_channels,
    float u,
    float v,
    float *output_rgba
) {
  float texel_x = u * static_cast<float>(source_width) - 0.5f;
  float texel_y = v * static_cast<float>(source_height) - 0.5f;

  int x0 = static_cast<int>(std::floor(texel_x));
  int y0 = static_cast<int>(std::floor(texel_y));
  int x1 = x0 + 1;
  int y1 = y0 + 1;

  float fractional_x = texel_x - static_cast<float>(x0);
  float fractional_y = texel_y - static_cast<float>(y0);

  x0 = ((x0 % source_width) + source_width) % source_width;
  x1 = ((x1 % source_width) + source_width) % source_width;
  y0 = std::clamp(y0, 0, source_height - 1);
  y1 = std::clamp(y1, 0, source_height - 1);

  auto fetch = [&](int x, int y, int channel) -> float {
    if (channel >= source_channels) {
      return (channel == 3) ? 1.0f : 0.0f;
    }
    return pixels[(y * source_width + x) * source_channels + channel];
  };

  for (int channel = 0; channel < 4; ++channel) {
    float top_left = fetch(x0, y0, channel);
    float top_right = fetch(x1, y0, channel);
    float bottom_left = fetch(x0, y1, channel);
    float bottom_right = fetch(x1, y1, channel);

    float top = top_left + fractional_x * (top_right - top_left);
    float bottom = bottom_left + fractional_x * (bottom_right - bottom_left);
    output_rgba[channel] = top + fractional_y * (bottom - top);
  }
}

} // namespace

CubemapConversionResult convert_equirectangular_to_cubemap(
    const float *equirectangular_pixels,
    int equirectangular_width,
    int equirectangular_height,
    int equirectangular_channels,
    uint32_t face_resolution
) {
  CubemapConversionResult result;

  for (uint32_t face = 0; face < 6; ++face) {
    auto &face_data = result.faces[face];
    face_data.width = face_resolution;
    face_data.height = face_resolution;
    face_data.pixels.resize(face_resolution * face_resolution * 4);

    for (uint32_t y = 0; y < face_resolution; ++y) {
      for (uint32_t x = 0; x < face_resolution; ++x) {
        float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(face_resolution);
        float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(face_resolution);

        Vec3 direction = normalize(cube_face_direction(face, u, v));

        float theta = std::atan2(direction.z, direction.x);
        float phi = std::asin(std::clamp(direction.y, -1.0f, 1.0f));

        float equirect_u = 0.5f + theta / (2.0f * PI);
        float equirect_v = 0.5f - phi / PI;

        size_t pixel_offset = (y * face_resolution + x) * 4;
        sample_equirectangular_bilinear(
            equirectangular_pixels,
            equirectangular_width,
            equirectangular_height,
            equirectangular_channels,
            equirect_u,
            equirect_v,
            &face_data.pixels[pixel_offset]
        );
      }
    }
  }

  return result;
}

namespace {

struct Vec3 sample_cubemap(const CubemapConversionResult &source, Vec3 direction) {
  float abs_x = std::fabs(direction.x);
  float abs_y = std::fabs(direction.y);
  float abs_z = std::fabs(direction.z);

  uint32_t face;
  float mapped_u, mapped_v;

  if (abs_x >= abs_y && abs_x >= abs_z) {
    if (direction.x > 0.0f) {
      face = 0;
      mapped_u = -direction.z / abs_x;
      mapped_v = -direction.y / abs_x;
    } else {
      face = 1;
      mapped_u = direction.z / abs_x;
      mapped_v = -direction.y / abs_x;
    }
  } else if (abs_y >= abs_x && abs_y >= abs_z) {
    if (direction.y > 0.0f) {
      face = 2;
      mapped_u = direction.x / abs_y;
      mapped_v = direction.z / abs_y;
    } else {
      face = 3;
      mapped_u = direction.x / abs_y;
      mapped_v = -direction.z / abs_y;
    }
  } else {
    if (direction.z > 0.0f) {
      face = 4;
      mapped_u = direction.x / abs_z;
      mapped_v = -direction.y / abs_z;
    } else {
      face = 5;
      mapped_u = -direction.x / abs_z;
      mapped_v = -direction.y / abs_z;
    }
  }

  float u = (mapped_u * 0.5f + 0.5f);
  float v = (mapped_v * 0.5f + 0.5f);

  const auto &face_data = source.faces[face];
  uint32_t texel_x = std::clamp(
      static_cast<uint32_t>(u * static_cast<float>(face_data.width)),
      0u, face_data.width - 1);
  uint32_t texel_y = std::clamp(
      static_cast<uint32_t>(v * static_cast<float>(face_data.height)),
      0u, face_data.height - 1);

  size_t offset = (texel_y * face_data.width + texel_x) * 4;
  return {
      face_data.pixels[offset + 0],
      face_data.pixels[offset + 1],
      face_data.pixels[offset + 2]};
}

float radical_inverse_vdc(uint32_t bits) {
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

} // namespace

CubemapConversionResult convolve_irradiance_cubemap(
    const CubemapConversionResult &source,
    uint32_t output_resolution,
    uint32_t hemisphere_samples
) {
  CubemapConversionResult result;

  for (uint32_t face = 0; face < 6; ++face) {
    auto &face_data = result.faces[face];
    face_data.width = output_resolution;
    face_data.height = output_resolution;
    face_data.pixels.resize(output_resolution * output_resolution * 4);

    for (uint32_t y = 0; y < output_resolution; ++y) {
      for (uint32_t x = 0; x < output_resolution; ++x) {
        float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(output_resolution);
        float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(output_resolution);

        Vec3 normal = normalize(cube_face_direction(face, u, v));

        Vec3 up = std::fabs(normal.y) < 0.999f
                      ? Vec3{0.0f, 1.0f, 0.0f}
                      : Vec3{0.0f, 0.0f, 1.0f};
        Vec3 tangent = normalize({
            up.y * normal.z - up.z * normal.y,
            up.z * normal.x - up.x * normal.z,
            up.x * normal.y - up.y * normal.x});
        Vec3 bitangent = {
            normal.y * tangent.z - normal.z * tangent.y,
            normal.z * tangent.x - normal.x * tangent.z,
            normal.x * tangent.y - normal.y * tangent.x};

        float accumulated_r = 0.0f;
        float accumulated_g = 0.0f;
        float accumulated_b = 0.0f;

        for (uint32_t sample_index = 0; sample_index < hemisphere_samples; ++sample_index) {
          float xi1 = static_cast<float>(sample_index) / static_cast<float>(hemisphere_samples);
          float xi2 = radical_inverse_vdc(sample_index);

          float phi = 2.0f * PI * xi2;
          float cos_theta = std::sqrt(1.0f - xi1);
          float sin_theta = std::sqrt(xi1);

          Vec3 tangent_sample = {
              sin_theta * std::cos(phi),
              sin_theta * std::sin(phi),
              cos_theta};

          Vec3 world_sample = normalize({
              tangent_sample.x * tangent.x + tangent_sample.y * bitangent.x + tangent_sample.z * normal.x,
              tangent_sample.x * tangent.y + tangent_sample.y * bitangent.y + tangent_sample.z * normal.y,
              tangent_sample.x * tangent.z + tangent_sample.y * bitangent.z + tangent_sample.z * normal.z});

          Vec3 radiance = sample_cubemap(source, world_sample);
          accumulated_r += radiance.x;
          accumulated_g += radiance.y;
          accumulated_b += radiance.z;
        }

        float inverse_count = 1.0f / static_cast<float>(hemisphere_samples);
        size_t pixel_offset = (y * output_resolution + x) * 4;
        face_data.pixels[pixel_offset + 0] = accumulated_r * inverse_count;
        face_data.pixels[pixel_offset + 1] = accumulated_g * inverse_count;
        face_data.pixels[pixel_offset + 2] = accumulated_b * inverse_count;
        face_data.pixels[pixel_offset + 3] = 1.0f;
      }
    }
  }

  return result;
}

} // namespace astralix
