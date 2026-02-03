#include "resource-component.hpp"
#include "components/resource/serializers/resource-component-serializer.hpp"
#include "guid.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/texture-descriptor.hpp"

namespace astralix {

ResourceComponent::ResourceComponent(COMPONENT_INIT_PARAMS)
    : COMPONENT_INIT(ResourceComponent, "resource", false,
                     create_ref<ResourceComponentSerializer>(this)) {};

void ResourceComponent::start() {
  if (!has_shader() && !m_shader_descriptor_id.empty()) {
    m_shader = resource_manager()->get_by_descriptor_id<Shader>(
        m_shader_descriptor_id);
  }

  if (has_shader()) {
    m_shader->bind();
  }
}

void ResourceComponent::update() {
  if (has_shader()) {
    m_shader->bind();

    auto resource_manager = ResourceManager::get();

    for (auto cubemap : m_cubemaps) {
      auto cubemap_ptr =
          resource_manager->get_by_descriptor_id<Texture3D>(cubemap.id);

      cubemap_ptr->active(cubemap_ptr->get_slot());

      m_shader->set_int(cubemap.name, cubemap_ptr->get_slot());

      cubemap_ptr->bind();
    }

    for (auto texture : m_textures) {
      auto texture_ptr =
          resource_manager->get_by_descriptor_id<Texture2D>(texture.id);

      auto slot_index = texture_ptr->id().index;

      texture_ptr->active(slot_index);

      m_shader->set_int(texture.name.c_str(), slot_index);

      texture_ptr->bind();
    }
  }
}

ResourceComponent *ResourceComponent::attach_shader(ResourceDescriptorID id) {
  auto resource_manager = ResourceManager::get();
  auto shader_ptr = resource_manager->get_by_descriptor_id<Shader>(id);

  m_shader = shader_ptr;
  m_shader_descriptor_id = id;

  return this;
}

ResourceComponent *ResourceComponent::set_shader(ResourceDescriptorID id) {
  auto resource_manager = ResourceManager::get();

  resource_manager->ensure_exists_by_descriptor_id<ShaderDescriptor>(id);

  m_shader_descriptor_id = id;
  m_shader =
      resource_manager->get_by_descriptor_id<Shader>(m_shader_descriptor_id);

  return this;
}

ResourceComponent *ResourceComponent::attach_texture(TextureRenderData data) {
  LOG_INFO("Attaching texture with ID: ", data.id);

  auto resource_manager = ResourceManager::get();

  resource_manager->ensure_exists_by_descriptor_id<Texture2DDescriptor>(
      data.id);

  m_textures.push_back(data);

  return this;
}

ResourceComponent *ResourceComponent::attach_cubemap(TextureRenderData data) {
  m_cubemaps.push_back(data);

  return this;
}

} // namespace astralix
