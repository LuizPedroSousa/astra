#pragma once

#include "base.hpp"
#include "guid.hpp"
#include "resources/descriptors/resource-descriptor.hpp"
#include <glm/glm.hpp>
#include <optional>
#include <vector>

namespace astralix {
struct MaterialDescriptor {
public:
  static Ref<MaterialDescriptor>
  create(const ResourceDescriptorID &id,
         std::vector<ResourceDescriptorID> diffuse_ids,
         std::vector<ResourceDescriptorID> specular_ids,
         std::optional<ResourceDescriptorID> normal_map_ids,
         std::optional<ResourceDescriptorID> displacement_map_ids,
         glm::vec3 emissive = glm::vec3(0.0f),
         float bloom_intensity = 0.0f);

  MaterialDescriptor(const ResourceDescriptorID &id,
                     std::vector<ResourceDescriptorID> diffuse_ids,
                     std::vector<ResourceDescriptorID> specular_ids,
                     std::optional<ResourceDescriptorID> normal_map_ids,
                     std::optional<ResourceDescriptorID> displacement_map_ids,
                     glm::vec3 emissive = glm::vec3(0.0f),
                     float bloom_intensity = 0.0f)
      : RESOURCE_DESCRIPTOR_INIT(), diffuse_ids(diffuse_ids),
        specular_ids(specular_ids), normal_map_ids(normal_map_ids),
        displacement_map_ids(displacement_map_ids), emissive(emissive),
        bloom_intensity(bloom_intensity) {}

  RESOURCE_DESCRIPTOR_PARAMS;

  std::vector<ResourceDescriptorID> diffuse_ids;
  std::vector<ResourceDescriptorID> specular_ids;
  std::optional<ResourceDescriptorID> normal_map_ids;
  std::optional<ResourceDescriptorID> displacement_map_ids;
  glm::vec3 emissive = glm::vec3(0.0f);
  float bloom_intensity = 0.0f;
};

} // namespace astralix
