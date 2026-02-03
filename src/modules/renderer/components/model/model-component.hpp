#pragma once
#include "components/component.hpp"
#include "guid.hpp"
#include "vector"

namespace astralix {

class ModelComponent : public Component<ModelComponent> {
public:
  ModelComponent(COMPONENT_INIT_PARAMS);

  void attach_model(ResourceDescriptorID id);

  void attach_models(std::initializer_list<ResourceDescriptorID> ids);

private:
  std::vector<ResourceDescriptorID> m_models;
};

} // namespace astralix
