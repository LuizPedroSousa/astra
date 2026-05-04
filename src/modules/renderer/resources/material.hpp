#pragma once

#include "base.hpp"
#include "guid.hpp"
#include "resource.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include <glm/glm.hpp>
#include <optional>

namespace astralix {

class Material : public Resource {
public:
  std::optional<ResourceDescriptorID> base_color;
  std::optional<ResourceDescriptorID> normal;
  std::optional<ResourceDescriptorID> metallic;
  std::optional<ResourceDescriptorID> roughness;
  std::optional<ResourceDescriptorID> metallic_roughness;
  std::optional<ResourceDescriptorID> occlusion;
  std::optional<ResourceDescriptorID> emissive;
  std::optional<ResourceDescriptorID> displacement;
  glm::vec4 base_color_factor = glm::vec4(1.0f);
  glm::vec3 emissive_factor = glm::vec3(0.0f);
  float metallic_factor = 1.0f;
  float roughness_factor = 1.0f;
  float occlusion_strength = 1.0f;
  float normal_scale = 1.0f;
  float height_scale = 0.02f;
  float bloom_intensity = 0.0f;
  bool alpha_mask = false;
  bool alpha_blend = false;
  float alpha_cutoff = 0.5f;
  bool double_sided = false;

  Material(RESOURCE_INIT_PARAMS, Ref<MaterialDescriptor> descriptor);

  static Ref<MaterialDescriptor> create(
      const ResourceDescriptorID &id,
      std::optional<ResourceDescriptorID> base_color = std::nullopt,
      std::optional<ResourceDescriptorID> normal = std::nullopt,
      std::optional<ResourceDescriptorID> metallic = std::nullopt,
      std::optional<ResourceDescriptorID> roughness = std::nullopt,
      std::optional<ResourceDescriptorID> metallic_roughness = std::nullopt,
      std::optional<ResourceDescriptorID> occlusion = std::nullopt,
      std::optional<ResourceDescriptorID> emissive = std::nullopt,
      std::optional<ResourceDescriptorID> displacement = std::nullopt,
      glm::vec4 base_color_factor = glm::vec4(1.0f),
      glm::vec3 emissive_factor = glm::vec3(0.0f),
      float metallic_factor = 1.0f,
      float roughness_factor = 1.0f,
      float occlusion_strength = 1.0f,
      float normal_scale = 1.0f,
      float height_scale = 0.02f,
      float bloom_intensity = 0.0f,
      bool alpha_mask = false,
      bool alpha_blend = false,
      float alpha_cutoff = 0.5f,
      bool double_sided = false);

  static Ref<MaterialDescriptor> define(
      const ResourceDescriptorID &id,
      std::optional<ResourceDescriptorID> base_color = std::nullopt,
      std::optional<ResourceDescriptorID> normal = std::nullopt,
      std::optional<ResourceDescriptorID> metallic = std::nullopt,
      std::optional<ResourceDescriptorID> roughness = std::nullopt,
      std::optional<ResourceDescriptorID> metallic_roughness = std::nullopt,
      std::optional<ResourceDescriptorID> occlusion = std::nullopt,
      std::optional<ResourceDescriptorID> emissive = std::nullopt,
      std::optional<ResourceDescriptorID> displacement = std::nullopt,
      glm::vec4 base_color_factor = glm::vec4(1.0f),
      glm::vec3 emissive_factor = glm::vec3(0.0f),
      float metallic_factor = 1.0f,
      float roughness_factor = 1.0f,
      float occlusion_strength = 1.0f,
      float normal_scale = 1.0f,
      float height_scale = 0.02f,
      float bloom_intensity = 0.0f,
      bool alpha_mask = false,
      bool alpha_blend = false,
      float alpha_cutoff = 0.5f,
      bool double_sided = false);

  static Ref<Material> from_descriptor(const ResourceHandle &id,
                                       Ref<MaterialDescriptor> descriptor);
};

} // namespace astralix
