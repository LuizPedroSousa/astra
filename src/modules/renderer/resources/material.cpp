#include "material.hpp"
#include "guid.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include <optional>

namespace astralix {

Material::Material(RESOURCE_INIT_PARAMS, Ref<MaterialDescriptor> descriptor)
    : RESOURCE_INIT(), diffuses(descriptor->diffuse_ids),
      speculars(descriptor->specular_ids),
      normal_map(descriptor->normal_map_ids),
      displacement_map(descriptor->displacement_map_ids),
      emissive(descriptor->emissive),
      bloom_intensity(descriptor->bloom_intensity) {};

Ref<MaterialDescriptor>
Material::create(const ResourceDescriptorID &id,
                 std::vector<ResourceDescriptorID> diffuse_ids,
                 std::vector<ResourceDescriptorID> specular_ids,
                 std::optional<ResourceDescriptorID> normal_map,
                 std::optional<ResourceDescriptorID> displacement_map,
                 glm::vec3 emissive,
                 float bloom_intensity) {
  return resource_manager()->register_material(MaterialDescriptor::create(
      id, diffuse_ids, specular_ids, normal_map, displacement_map, emissive,
      bloom_intensity));
};

Ref<MaterialDescriptor>
Material::define(const ResourceDescriptorID &id,
                 std::vector<ResourceDescriptorID> diffuse_ids,
                 std::vector<ResourceDescriptorID> specular_ids,
                 std::optional<ResourceDescriptorID> normal_map,
                 std::optional<ResourceDescriptorID> displacement_map,
                 glm::vec3 emissive,
                 float bloom_intensity) {
  return MaterialDescriptor::create(id, diffuse_ids, specular_ids, normal_map,
                                    displacement_map, emissive,
                                    bloom_intensity);
};

Ref<Material> Material::from_descriptor(const ResourceHandle &id,
                                        Ref<MaterialDescriptor> descriptor) {
  return create_ref<Material>(id, descriptor);
};

} // namespace astralix
