#pragma once

#include "managers/resource-manager.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "resources/texture.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace astralix::rendering {

inline constexpr uint32_t k_brdf_lut_size = 512u;

inline const ResourceDescriptorID &brdf_lut_texture_id() {
  static const ResourceDescriptorID id = "textures::ibl::brdf_lut";
  return id;
}

inline const ResourceDescriptorID &default_brdf_lut_fallback_texture_id() {
  static const ResourceDescriptorID id = "textures::ibl::brdf_lut_fallback";
  return id;
}

namespace brdf_detail {

inline float radical_inverse_vdc(uint32_t bits) {
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

inline void hammersley(uint32_t index, uint32_t total, float &xi0, float &xi1) {
  xi0 = static_cast<float>(index) / static_cast<float>(total);
  xi1 = radical_inverse_vdc(index);
}

inline void importance_sample_ggx(
    float xi0, float xi1, float roughness,
    float &hx, float &hy, float &hz
) {
  float alpha = roughness * roughness;
  float phi = 2.0f * 3.14159265359f * xi0;
  float cos_theta = std::sqrt((1.0f - xi1) / (1.0f + (alpha * alpha - 1.0f) * xi1));
  float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);
  hx = std::cos(phi) * sin_theta;
  hy = std::sin(phi) * sin_theta;
  hz = cos_theta;
}

inline float geometry_schlick_ggx_ibl(float ndot, float roughness) {
  float k = (roughness * roughness) / 2.0f;
  return ndot / (ndot * (1.0f - k) + k);
}

inline float geometry_smith_ibl(float n_dot_v, float n_dot_l, float roughness) {
  return geometry_schlick_ggx_ibl(n_dot_v, roughness) *
         geometry_schlick_ggx_ibl(n_dot_l, roughness);
}

inline void integrate_brdf(float n_dot_v, float roughness, float &scale, float &bias) {
  constexpr uint32_t sample_count = 1024u;

  float vx = std::sqrt(1.0f - n_dot_v * n_dot_v);
  float vy = 0.0f;
  float vz = n_dot_v;

  float accumulated_scale = 0.0f;
  float accumulated_bias = 0.0f;

  for (uint32_t i = 0; i < sample_count; ++i) {
    float xi0, xi1;
    hammersley(i, sample_count, xi0, xi1);

    float hx, hy, hz;
    importance_sample_ggx(xi0, xi1, roughness, hx, hy, hz);

    float l_dot = 2.0f * (vx * hx + vy * hy + vz * hz);
    float lx = l_dot * hx - vx;
    float ly = l_dot * hy - vy;
    float lz = l_dot * hz - vz;

    float n_dot_l = std::max(lz, 0.0f);
    float n_dot_h = std::max(hz, 0.0f);
    float v_dot_h = std::max(vx * hx + vy * hy + vz * hz, 0.0f);

    if (n_dot_l > 0.0f) {
      float geometry = geometry_smith_ibl(n_dot_v, n_dot_l, roughness);
      float geometry_vis = (geometry * v_dot_h) / (n_dot_h * n_dot_v + 1e-4f);
      float fresnel_coefficient = std::pow(1.0f - v_dot_h, 5.0f);

      accumulated_scale += geometry_vis * (1.0f - fresnel_coefficient);
      accumulated_bias += geometry_vis * fresnel_coefficient;
    }
  }

  scale = accumulated_scale / static_cast<float>(sample_count);
  bias = accumulated_bias / static_cast<float>(sample_count);
}

} // namespace brdf_detail

inline std::vector<unsigned char> generate_brdf_lut_rgba(uint32_t size = k_brdf_lut_size) {
  std::vector<unsigned char> pixels(size * size * 4);

  for (uint32_t y = 0; y < size; ++y) {
    for (uint32_t x = 0; x < size; ++x) {
      float n_dot_v = std::max(
          (static_cast<float>(x) + 0.5f) / static_cast<float>(size), 1.0f / static_cast<float>(size)
      );
      float roughness = std::max(
          (static_cast<float>(y) + 0.5f) / static_cast<float>(size), 0.045f
      );

      float scale, bias;
      brdf_detail::integrate_brdf(n_dot_v, roughness, scale, bias);

      uint32_t offset = (y * size + x) * 4;
      pixels[offset + 0] = static_cast<unsigned char>(std::min(scale * 255.0f, 255.0f));
      pixels[offset + 1] = static_cast<unsigned char>(std::min(bias * 255.0f, 255.0f));
      pixels[offset + 2] = 0;
      pixels[offset + 3] = 255;
    }
  }

  return pixels;
}

inline TextureConfig make_brdf_lut_config(
    uint32_t width,
    uint32_t height,
    unsigned char *buffer
) {
  TextureConfig config;
  config.width = width;
  config.height = height;
  config.bitmap = false;
  config.format = TextureFormat::RGBA;
  config.buffer = buffer;
  config.parameters = {
      {TextureParameter::WrapS, TextureValue::ClampToEdge},
      {TextureParameter::WrapT, TextureValue::ClampToEdge},
      {TextureParameter::MagFilter, TextureValue::Linear},
      {TextureParameter::MinFilter, TextureValue::Linear},
  };
  return config;
}

inline Ref<Texture2DDescriptor> ensure_default_brdf_lut_fallback() {
  auto descriptor = resource_manager()->get_descriptor_by_id<Texture2DDescriptor>(
      default_brdf_lut_fallback_texture_id()
  );
  if (descriptor != nullptr) {
    return descriptor;
  }

  static const std::array<unsigned char, 4> k_fallback_pixel = {
      255,
      0,
      0,
      255,
  };

  return Texture2D::create(
      default_brdf_lut_fallback_texture_id(),
      make_brdf_lut_config(1u, 1u, const_cast<unsigned char *>(k_fallback_pixel.data()))
  );
}

inline Ref<Texture2DDescriptor> ensure_brdf_lut() {
  auto descriptor = resource_manager()->get_descriptor_by_id<Texture2DDescriptor>(brdf_lut_texture_id());
  if (descriptor != nullptr) {
    return descriptor;
  }

  static auto lut_pixels = generate_brdf_lut_rgba();
  return Texture2D::create(
      brdf_lut_texture_id(),
      make_brdf_lut_config(
          k_brdf_lut_size,
          k_brdf_lut_size,
          lut_pixels.data()
      )
  );
}

} // namespace astralix::rendering
