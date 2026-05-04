#pragma once
#include "filesystem"
#include "guid.hpp"
#include "mesh.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include "resources/resource.hpp"
#include "vector"

namespace astralix {

class Model : public Resource {
public:
  Model(RESOURCE_INIT_PARAMS,
        std::vector<Mesh> meshes,
        std::vector<ResourceDescriptorID> materials,
        std::vector<uint32_t> material_slots = {})
      : RESOURCE_INIT(),
        meshes(std::move(meshes)),
        materials(std::move(materials)),
        material_slots(std::move(material_slots)) {};

  static Ref<ModelDescriptor> create(const ResourceDescriptorID &id,
                                     Ref<Path> source_path,
                                     ModelImportSettings import_settings = {},
                                     std::vector<ResourceDescriptorID> material_ids = {});
  static Ref<ModelDescriptor> define(const ResourceDescriptorID &id,
                                     Ref<Path> source_path,
                                     ModelImportSettings import_settings = {},
                                     std::vector<ResourceDescriptorID> material_ids = {});
  static Ref<Model> from_descriptor(const ResourceHandle &id,
                                    Ref<ModelDescriptor> descriptor);
  static Ref<Model> from_imported_data(
      const ResourceHandle &id,
      Ref<ModelDescriptor> descriptor,
      ImportedModelData imported
  );

  std::vector<Mesh> meshes;
  std::vector<ResourceDescriptorID> materials;
  std::vector<uint32_t> material_slots;
};

} // namespace astralix
