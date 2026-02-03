#include "material-component.hpp"
#include "base.hpp"
#include "components/material/serializers/material-component-serializer.hpp"
#include "components/resource/resource-component.hpp"
#include "guid.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/material-descriptor.hpp"

namespace astralix {

MaterialComponent::MaterialComponent(COMPONENT_INIT_PARAMS)
    : COMPONENT_INIT(MaterialComponent, "material", true,
                     create_ref<MaterialComponentSerializer>(this)) {};

void MaterialComponent::reset_material() {}

void MaterialComponent::update() {
  CHECK_ACTIVE(this);
  auto owner = get_owner();

  owner->get_component<ResourceComponent>()->shader()->set_float(
      "material.shininess", 32.0f);
}

void MaterialComponent::attach_material(ResourceDescriptorID material_id) {
  auto owner = get_owner();
  auto resource = owner->get_component<ResourceComponent>();

  auto resource_manager = ResourceManager::get();

  auto material =
      resource_manager->get_by_descriptor_id<MaterialDescriptor>(material_id);

  for (int i = 0; i < material->diffuse_ids.size(); i++) {
    resource->attach_texture(
        {material->diffuse_ids[i], get_name("diffuse", i)});
  }

  if (material->normal_map_ids) {
    resource->attach_texture({*material->normal_map_ids, "normal_map"});
  }

  if (material->displacement_map_ids) {
    resource->attach_texture(
        {*material->displacement_map_ids, "displacement_map"});
  }

  for (int i = 0; i < material->specular_ids.size(); i++) {
    resource->attach_texture(
        {material->specular_ids[i], get_name("specular", i)});
  }
}

std::string MaterialComponent::get_name(const char *prefix, int count) {
  std::string result =
      std::string("materials" + std::string("[") + std::to_string(count) +
                  std::string("]") + "." + prefix);

  return result;
}

void MaterialComponent::attach_materials(
    std::vector<ResourceDescriptorID> material_ids) {
  for (int i = 0; i < material_ids.size(); i++) {
    attach_material(material_ids[i]);
  }
}

} // namespace astralix
