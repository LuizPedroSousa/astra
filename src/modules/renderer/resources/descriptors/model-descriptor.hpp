#pragma once

#include "base.hpp"
#include "guid.hpp"
#include "importers/model-importer.hpp"
#include "path.hpp"
#include "resources/descriptors/resource-descriptor.hpp"

#include <vector>

namespace astralix {
struct ModelDescriptor {
public:
  static Ref<ModelDescriptor> create(const ResourceDescriptorID &id,
                                     Ref<Path> source_path,
                                     ModelImportSettings import_settings = {},
                                     std::vector<ResourceDescriptorID> material_ids = {});

  ModelDescriptor(
      const ResourceDescriptorID &id,
      Ref<Path> source_path,
      ModelImportSettings import_settings = {},
      std::vector<ResourceDescriptorID> material_ids = {}
  )
      : RESOURCE_DESCRIPTOR_INIT(),
        source_path(std::move(source_path)),
        import_settings(import_settings),
        material_ids(std::move(material_ids)) {}

  RESOURCE_DESCRIPTOR_PARAMS;
  Ref<Path> source_path;
  ModelImportSettings import_settings;
  std::vector<ResourceDescriptorID> material_ids;
};

} // namespace astralix
