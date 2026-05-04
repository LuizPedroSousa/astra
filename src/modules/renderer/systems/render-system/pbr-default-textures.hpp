#pragma once

#include "guid.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "resources/texture.hpp"
#include <array>

namespace astralix::rendering {

inline const ResourceDescriptorID &default_pbr_base_color_texture_id() {
  static const ResourceDescriptorID id =
      "textures::defaults::pbr_base_color_white";
  return id;
}

inline const ResourceDescriptorID &default_pbr_normal_texture_id() {
  static const ResourceDescriptorID id = "textures::defaults::pbr_normal_flat";
  return id;
}

inline const ResourceDescriptorID &default_pbr_metallic_texture_id() {
  static const ResourceDescriptorID id = "textures::defaults::pbr_metallic_black";
  return id;
}

inline const ResourceDescriptorID &default_pbr_roughness_texture_id() {
  static const ResourceDescriptorID id = "textures::defaults::pbr_roughness_white";
  return id;
}

inline const ResourceDescriptorID &default_pbr_metallic_roughness_texture_id() {
  static const ResourceDescriptorID id =
      "textures::defaults::pbr_metallic_roughness_neutral";
  return id;
}

inline const ResourceDescriptorID &default_pbr_occlusion_texture_id() {
  static const ResourceDescriptorID id =
      "textures::defaults::pbr_occlusion_white";
  return id;
}

inline const ResourceDescriptorID &default_pbr_emissive_texture_id() {
  static const ResourceDescriptorID id =
      "textures::defaults::pbr_emissive_identity";
  return id;
}

inline const ResourceDescriptorID &default_pbr_displacement_texture_id() {
  static const ResourceDescriptorID id =
      "textures::defaults::pbr_displacement_black";
  return id;
}

inline std::array<ResourceDescriptorID, 8> default_pbr_texture_ids() {
  return {
      default_pbr_base_color_texture_id(),
      default_pbr_normal_texture_id(),
      default_pbr_metallic_texture_id(),
      default_pbr_roughness_texture_id(),
      default_pbr_metallic_roughness_texture_id(),
      default_pbr_occlusion_texture_id(),
      default_pbr_emissive_texture_id(),
      default_pbr_displacement_texture_id(),
  };
}

inline Ref<Texture2DDescriptor> ensure_default_pbr_texture(
    const ResourceDescriptorID &id, const std::array<unsigned char, 4> &rgba
) {
  auto descriptor = resource_manager()->get_descriptor_by_id<Texture2DDescriptor>(id);
  if (descriptor != nullptr) {
    return descriptor;
  }

  TextureConfig config;
  config.width = 1;
  config.height = 1;
  config.bitmap = false;
  config.format = TextureFormat::RGBA;
  config.buffer = const_cast<unsigned char *>(rgba.data());
  config.parameters = {
      {TextureParameter::WrapS, TextureValue::Repeat},
      {TextureParameter::WrapT, TextureValue::Repeat},
      {TextureParameter::MagFilter, TextureValue::Nearest},
      {TextureParameter::MinFilter, TextureValue::Nearest},
  };
  return Texture2D::create(id, config);
}

inline void ensure_pbr_default_textures() {
  static const std::array<unsigned char, 4> k_white = {255, 255, 255, 255};
  static const std::array<unsigned char, 4> k_flat_normal = {128, 128, 255, 255};
  static const std::array<unsigned char, 4> k_metallic_roughness = {
      255,
      255,
      0,
      255,
  };
  static const std::array<unsigned char, 4> k_black = {0, 0, 0, 255};

  ensure_default_pbr_texture(default_pbr_base_color_texture_id(), k_white);
  ensure_default_pbr_texture(default_pbr_normal_texture_id(), k_flat_normal);
  // Neutral metallic fallback must be black so materials without a metallic
  // map stay dielectric instead of becoming chrome.
  ensure_default_pbr_texture(default_pbr_metallic_texture_id(), k_black);
  ensure_default_pbr_texture(default_pbr_roughness_texture_id(), k_white);
  ensure_default_pbr_texture(
      default_pbr_metallic_roughness_texture_id(), k_metallic_roughness
  );
  ensure_default_pbr_texture(default_pbr_occlusion_texture_id(), k_white);
  // Use multiplicative identity defaults so scalar factors still work when
  // materials omit optional textures.
  ensure_default_pbr_texture(default_pbr_emissive_texture_id(), k_white);
  ensure_default_pbr_texture(default_pbr_displacement_texture_id(), k_black);
}

inline const ResourceDescriptorID &default_ibl_black_cubemap_id() {
  static const ResourceDescriptorID id =
      "textures::defaults::ibl_black_cubemap";
  return id;
}

inline Ref<Texture3DDescriptor> ensure_default_ibl_cubemap() {
  auto descriptor = resource_manager()->get_descriptor_by_id<Texture3DDescriptor>(
      default_ibl_black_cubemap_id()
  );
  if (descriptor != nullptr) {
    return descriptor;
  }

  static const std::array<unsigned char, 4> k_black_pixel = {0, 0, 0, 255};
  static const std::vector<const unsigned char *> face_buffers(
      6, k_black_pixel.data()
  );

  return Texture3D::create_from_buffer(
      default_ibl_black_cubemap_id(), 1, 1, face_buffers
  );
}

} // namespace astralix::rendering
