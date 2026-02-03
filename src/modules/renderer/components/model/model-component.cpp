#include "components/material/material-component.hpp"

#include "components/mesh/mesh-component.hpp"
#include "components/model/model-component.hpp"
#include "components/model/serializers/model-component-serializer.hpp"
#include "guid.hpp"
#include "managers/resource-manager.hpp"

namespace astralix {

ModelComponent::ModelComponent(COMPONENT_INIT_PARAMS)
    : COMPONENT_INIT(ModelComponent, "model", true,
                     create_ref<ModelComponentSerializer>(this)) {}

void ModelComponent::attach_model(ResourceDescriptorID id) {
  auto resource_manager = ResourceManager::get();

  auto model = resource_manager->get_by_descriptor_id<Model>(id);

  auto owner = get_owner();

  owner->get_or_add_component<MeshComponent>()->attach_meshes(model->meshes);

  owner->get_or_add_component<MaterialComponent>()->attach_materials(
      model->materials);

  m_models.push_back(id);
};

void ModelComponent::attach_models(
    std::initializer_list<ResourceDescriptorID> ids) {
  for (auto &id : ids) {
    attach_model(id);
  }
};

} // namespace astralix
