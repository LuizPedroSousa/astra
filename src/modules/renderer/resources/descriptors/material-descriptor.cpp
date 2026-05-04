#include "resources/descriptors/material-descriptor.hpp"

namespace astralix {

Ref<MaterialDescriptor> MaterialDescriptor::create(
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
    float height_scale,
    float bloom_intensity,
    bool alpha_mask,
    bool alpha_blend,
    float alpha_cutoff,
    bool double_sided) {
  return create_ref<MaterialDescriptor>(
      id,
      std::move(base_color_id),
      std::move(normal_id),
      std::move(metallic_id),
      std::move(roughness_id),
      std::move(metallic_roughness_id),
      std::move(occlusion_id),
      std::move(emissive_id),
      std::move(displacement_id),
      base_color_factor,
      emissive_factor,
      metallic_factor,
      roughness_factor,
      occlusion_strength,
      normal_scale,
      height_scale,
      bloom_intensity,
      alpha_mask,
      alpha_blend,
      alpha_cutoff,
      double_sided);
}

} // namespace astralix
