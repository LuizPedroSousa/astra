#include "resources/descriptors/model-descriptor.hpp"
#include "base.hpp"
#include "guid.hpp"

namespace astralix {

Ref<ModelDescriptor> ModelDescriptor::create(const ResourceDescriptorID &id,
                                             Ref<Path> source_path,
                                             ModelImportSettings import_settings,
                                             std::vector<ResourceDescriptorID> material_ids) {
  return create_ref<ModelDescriptor>(
      id,
      std::move(source_path),
      import_settings,
      std::move(material_ids)
  );
}

} // namespace astralix
