#include "resources/model.hpp"

namespace astralix {

Ref<Model> Model::from_descriptor(const ResourceHandle &id,
                                  Ref<ModelDescriptor> descriptor) {
  (void)descriptor;
  return create_ref<Model>(id, std::vector<Mesh>{},
                           std::vector<ResourceDescriptorID>{});
}

} // namespace astralix
