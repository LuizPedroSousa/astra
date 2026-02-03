#pragma once
#include "base.hpp"
#include "guid.hpp"
#include "resource.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include "vector"
#include <optional>

namespace astralix {

class Material : public Resource {
public:
  std::vector<ResourceDescriptorID> diffuses;
  std::vector<ResourceDescriptorID> speculars;
  std::optional<ResourceDescriptorID> normal_map;
  std::optional<ResourceDescriptorID> displacement_map;

  Material(RESOURCE_INIT_PARAMS, Ref<MaterialDescriptor> descriptor);

  static Ref<MaterialDescriptor>
  create(const ResourceDescriptorID &id,
         std::vector<ResourceDescriptorID> diffuse_ids = {},
         std::vector<ResourceDescriptorID> specular_ids = {},
         std::optional<ResourceDescriptorID> normal_map = std::nullopt,
         std::optional<ResourceDescriptorID> displacement_map = std::nullopt);

  static Ref<MaterialDescriptor>
  define(const ResourceDescriptorID &id,
         std::vector<ResourceDescriptorID> diffuse_ids = {},
         std::vector<ResourceDescriptorID> specular_ids = {},
         std::optional<ResourceDescriptorID> normal_map = std::nullopt,
         std::optional<ResourceDescriptorID> displacement_map = std::nullopt);

  static Ref<Material> from_descriptor(const ResourceHandle &id,
                                       Ref<MaterialDescriptor> descriptor);
};

} // namespace astralix
