#include "resource-manager.hpp"
#include "assert.hpp"
#include "glad/glad.h"
#include "guid.hpp"
#include "log.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "resources/shader.hpp"

namespace astralix {
static int count = 0;

Ref<Texture2DDescriptor>
ResourceManager::register_texture(Ref<Texture2DDescriptor> descriptor) {
  return m_texture_2d_descriptor_pool.get(
      m_texture_2d_descriptor_pool.register_or_get(descriptor));
}

Ref<Texture3DDescriptor>
ResourceManager::register_texture(Ref<Texture3DDescriptor> descriptor) {
  return m_texture_3d_descriptor_pool.get(
      m_texture_3d_descriptor_pool.register_or_get(descriptor));
}

void ResourceManager::register_textures(
    std::initializer_list<Ref<Texture2DDescriptor>> descriptors) {
  for (auto texture : descriptors) {
    register_texture(texture);
  }
}

void ResourceManager::register_textures(
    std::initializer_list<Ref<Texture3DDescriptor>> descriptors) {
  for (auto texture : descriptors) {
    register_texture(texture);
  }
}

Ref<ShaderDescriptor>
ResourceManager::register_shader(Ref<ShaderDescriptor> descriptor) {
  return m_shader_descriptor_pool.get(
      m_shader_descriptor_pool.register_or_get(descriptor));
}

void ResourceManager::register_shaders(
    std::initializer_list<Ref<ShaderDescriptor>> descriptors) {
  for (auto descriptor : descriptors) {
    register_shader(descriptor);
  }
}

Ref<ModelDescriptor>
ResourceManager::register_model(Ref<ModelDescriptor> descriptor) {
  return m_model_descriptor_pool.get(
      m_model_descriptor_pool.register_or_get(descriptor));
}

void ResourceManager::register_models(
    std::initializer_list<Ref<ModelDescriptor>> models) {
  for (auto &model : models) {
    register_model(model);
  }
}

void ResourceManager::register_fonts(
    std::initializer_list<Ref<FontDescriptor>> fonts) {
  for (auto font : fonts) {
    register_font(font);
  }
}

Ref<FontDescriptor>
ResourceManager::register_font(Ref<FontDescriptor> descriptor) {
  return m_font_descriptor_pool.get(
      m_font_descriptor_pool.register_or_get(descriptor));
}

Ref<MaterialDescriptor>
ResourceManager::register_material(Ref<MaterialDescriptor> material) {
  return m_material_descriptor_pool.get(
      m_material_descriptor_pool.register_or_get(material));
}

void ResourceManager::register_materials(
    std::initializer_list<Ref<MaterialDescriptor>> materials) {
  for (auto material : materials) {
    register_material(material);
  }
};

std::vector<Model *> ResourceManager::get_model_by_descriptor_ids(
    std::initializer_list<ResourceDescriptorID> ids) {
  std::vector<Model *> models;

  for (auto id : ids) {
    auto model = get_by_descriptor_id<Model>(id);

    if (model != nullptr) {
      models.push_back(model.get());
    }
  }

  return models;
}

} // namespace astralix
