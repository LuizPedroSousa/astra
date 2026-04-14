#pragma once

#include "base.hpp"
#include "guid.hpp"
#include "resources/descriptors/resource-descriptor.hpp"
#include <glm/glm.hpp>
#include <optional>

namespace astralix {

struct MaterialDescriptor {
public:
  static Ref<MaterialDescriptor> create(
      const ResourceDescriptorID &id,
      std::optional<ResourceDescriptorID> base_color_id = std::nullopt,
      std::optional<ResourceDescriptorID> normal_id = std::nullopt,
      std::optional<ResourceDescriptorID> metallic_id = std::nullopt,
      std::optional<ResourceDescriptorID> roughness_id = std::nullopt,
      std::optional<ResourceDescriptorID> metallic_roughness_id = std::nullopt,
      std::optional<ResourceDescriptorID> occlusion_id = std::nullopt,
      std::optional<ResourceDescriptorID> emissive_id = std::nullopt,
      std::optional<ResourceDescriptorID> displacement_id = std::nullopt,
      glm::vec4 base_color_factor = glm::vec4(1.0f),
      glm::vec3 emissive_factor = glm::vec3(0.0f),
      float metallic_factor = 1.0f,
      float roughness_factor = 1.0f,
      float occlusion_strength = 1.0f,
      float normal_scale = 1.0f,
      float bloom_intensity = 0.0f);

  MaterialDescriptor(
      const ResourceDescriptorID &id,
      std::optional<ResourceDescriptorID> base_color_id,
      std::optional<ResourceDescriptorID> normal_id,
      std::optional<ResourceDescriptorID> metallic_id,
      std::optional<ResourceDescriptorID> roughness_id,
      std::optional<ResourceDescriptorID> metallic_roughness_id,
      std::optional<ResourceDescriptorID> occlusion_id,
      std::optional<ResourceDescriptorID> emissive_id,
      std::optional<ResourceDescriptorID> displacement_id,
      glm::vec4 base_color_factor,
      glm::vec3 emissive_factor,
      float metallic_factor,
      float roughness_factor,
      float occlusion_strength,
      float normal_scale,
      float bloom_intensity)
      : RESOURCE_DESCRIPTOR_INIT(), base_color_id(std::move(base_color_id)),
        normal_id(std::move(normal_id)),
        metallic_id(std::move(metallic_id)),
        roughness_id(std::move(roughness_id)),
        metallic_roughness_id(std::move(metallic_roughness_id)),
        occlusion_id(std::move(occlusion_id)),
        emissive_id(std::move(emissive_id)),
        displacement_id(std::move(displacement_id)),
        base_color_factor(base_color_factor), emissive_factor(emissive_factor),
        metallic_factor(metallic_factor), roughness_factor(roughness_factor),
        occlusion_strength(occlusion_strength), normal_scale(normal_scale),
        bloom_intensity(bloom_intensity) {}

  RESOURCE_DESCRIPTOR_PARAMS;

  std::optional<ResourceDescriptorID> base_color_id;
  std::optional<ResourceDescriptorID> normal_id;
  std::optional<ResourceDescriptorID> metallic_id;
  std::optional<ResourceDescriptorID> roughness_id;
  std::optional<ResourceDescriptorID> metallic_roughness_id;
  std::optional<ResourceDescriptorID> occlusion_id;
  std::optional<ResourceDescriptorID> emissive_id;
  std::optional<ResourceDescriptorID> displacement_id;
  glm::vec4 base_color_factor = glm::vec4(1.0f);
  glm::vec3 emissive_factor = glm::vec3(0.0f);
  float metallic_factor = 1.0f;
  float roughness_factor = 1.0f;
  float occlusion_strength = 1.0f;
  float normal_scale = 1.0f;
  float bloom_intensity = 0.0f;
};

} // namespace astralix
