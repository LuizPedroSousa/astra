#include "material.hpp"

#include "managers/resource-manager.hpp"

namespace astralix {

Material::Material(RESOURCE_INIT_PARAMS, Ref<MaterialDescriptor> descriptor)
    : RESOURCE_INIT(), base_color(descriptor->base_color_id),
      normal(descriptor->normal_id),
      metallic(descriptor->metallic_id),
      roughness(descriptor->roughness_id),
      metallic_roughness(descriptor->metallic_roughness_id),
      occlusion(descriptor->occlusion_id), emissive(descriptor->emissive_id),
      displacement(descriptor->displacement_id),
      base_color_factor(descriptor->base_color_factor),
      emissive_factor(descriptor->emissive_factor),
      metallic_factor(descriptor->metallic_factor),
      roughness_factor(descriptor->roughness_factor),
      occlusion_strength(descriptor->occlusion_strength),
      normal_scale(descriptor->normal_scale),
      bloom_intensity(descriptor->bloom_intensity) {}

Ref<MaterialDescriptor> Material::create(
    const ResourceDescriptorID &id,
    std::optional<ResourceDescriptorID> base_color,
    std::optional<ResourceDescriptorID> normal,
    std::optional<ResourceDescriptorID> metallic,
    std::optional<ResourceDescriptorID> roughness,
    std::optional<ResourceDescriptorID> metallic_roughness,
    std::optional<ResourceDescriptorID> occlusion,
    std::optional<ResourceDescriptorID> emissive,
    std::optional<ResourceDescriptorID> displacement,
    glm::vec4 base_color_factor,
    glm::vec3 emissive_factor,
    float metallic_factor,
    float roughness_factor,
    float occlusion_strength,
    float normal_scale,
    float bloom_intensity) {
  return resource_manager()->register_material(MaterialDescriptor::create(
      id,
      std::move(base_color),
      std::move(normal),
      std::move(metallic),
      std::move(roughness),
      std::move(metallic_roughness),
      std::move(occlusion),
      std::move(emissive),
      std::move(displacement),
      base_color_factor,
      emissive_factor,
      metallic_factor,
      roughness_factor,
      occlusion_strength,
      normal_scale,
      bloom_intensity));
}

Ref<MaterialDescriptor> Material::define(
    const ResourceDescriptorID &id,
    std::optional<ResourceDescriptorID> base_color,
    std::optional<ResourceDescriptorID> normal,
    std::optional<ResourceDescriptorID> metallic,
    std::optional<ResourceDescriptorID> roughness,
    std::optional<ResourceDescriptorID> metallic_roughness,
    std::optional<ResourceDescriptorID> occlusion,
    std::optional<ResourceDescriptorID> emissive,
    std::optional<ResourceDescriptorID> displacement,
    glm::vec4 base_color_factor,
    glm::vec3 emissive_factor,
    float metallic_factor,
    float roughness_factor,
    float occlusion_strength,
    float normal_scale,
    float bloom_intensity) {
  return MaterialDescriptor::create(
      id,
      std::move(base_color),
      std::move(normal),
      std::move(metallic),
      std::move(roughness),
      std::move(metallic_roughness),
      std::move(occlusion),
      std::move(emissive),
      std::move(displacement),
      base_color_factor,
      emissive_factor,
      metallic_factor,
      roughness_factor,
      occlusion_strength,
      normal_scale,
      bloom_intensity);
}

Ref<Material> Material::from_descriptor(const ResourceHandle &id,
                                        Ref<MaterialDescriptor> descriptor) {
  return create_ref<Material>(id, descriptor);
}

} // namespace astralix
