#include "model.hpp"

#include "assert.hpp"
#include "guid.hpp"
#include "importers/model-importer.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/model-descriptor.hpp"

namespace astralix {

Ref<ModelDescriptor> Model::create(const ResourceDescriptorID &id,
                                   Ref<Path> source_path,
                                   ModelImportSettings import_settings,
                                   std::vector<ResourceDescriptorID> material_ids) {
  return resource_manager()->register_model(ModelDescriptor::create(
      id,
      std::move(source_path),
      import_settings,
      std::move(material_ids)
  ));
};

Ref<ModelDescriptor> Model::define(const ResourceDescriptorID &id,
                                   Ref<Path> source_path,
                                   ModelImportSettings import_settings,
                                   std::vector<ResourceDescriptorID> material_ids) {
  return ModelDescriptor::create(
      id,
      std::move(source_path),
      import_settings,
      std::move(material_ids)
  );
}

Ref<Model> Model::from_descriptor(const ResourceHandle &id,
                                  Ref<ModelDescriptor> descriptor) {
  const auto full_path = path_manager()->resolve(descriptor->source_path);
  auto imported = import_model_file(full_path, descriptor->import_settings);

  return from_imported_data(id, descriptor, std::move(imported));
};

Ref<Model> Model::from_imported_data(
    const ResourceHandle &id,
    Ref<ModelDescriptor> descriptor,
    ImportedModelData imported
) {

  ASTRA_ENSURE(
      !descriptor->material_ids.empty() &&
          descriptor->material_ids.size() != imported.material_slot_count,
      "Model '",
      descriptor->id,
      "' declares ",
      descriptor->material_ids.size(),
      " material id(s) but importer found ",
      imported.material_slot_count,
      " material slot(s)"
  );

  return create_ref<Model>(
      id,
      std::move(imported.meshes),
      descriptor->material_ids,
      std::move(imported.mesh_material_slots)
  );
}

} // namespace astralix
