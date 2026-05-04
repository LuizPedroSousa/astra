#include "resource-manager.hpp"
#include "assert.hpp"
#include "glad/glad.h"
#include "guid.hpp"
#include "log.hpp"
#include "managers/path-manager.hpp"
#include "resources/model.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "resources/shader.hpp"
#include "systems/job-system/job-system.hpp"
#include <array>
#include <memory>
#include <utility>

namespace astralix {

namespace {

template <typename F>
class ScopeExit {
public:
  explicit ScopeExit(F &&fn) : m_fn(std::forward<F>(fn)) {}
  ~ScopeExit() { m_fn(); }

  ScopeExit(const ScopeExit &) = delete;
  ScopeExit &operator=(const ScopeExit &) = delete;

private:
  F m_fn;
};

template <typename F>
ScopeExit<F> make_scope_exit(F &&fn) {
  return ScopeExit<F>(std::forward<F>(fn));
}

struct PendingTexture2DLoad {
  PreparedTexture2DData prepared;
};

struct PendingModelLoad {
  ImportedModelData imported;
};

} // namespace

Ref<Texture2DDescriptor>
ResourceManager::register_texture(Ref<Texture2DDescriptor> descriptor) {
  return m_texture_2d_descriptor_pool.get(
      m_texture_2d_descriptor_pool.register_or_get(descriptor)
  );
}

Ref<Texture3DDescriptor>
ResourceManager::register_texture(Ref<Texture3DDescriptor> descriptor) {
  return m_texture_3d_descriptor_pool.get(
      m_texture_3d_descriptor_pool.register_or_get(descriptor)
  );
}

void ResourceManager::register_textures(
    std::initializer_list<Ref<Texture2DDescriptor>> descriptors
) {
  for (auto texture : descriptors) {
    register_texture(texture);
  }
}

void ResourceManager::register_textures(
    std::initializer_list<Ref<Texture3DDescriptor>> descriptors
) {
  for (auto texture : descriptors) {
    register_texture(texture);
  }
}

Ref<ShaderDescriptor>
ResourceManager::register_shader(Ref<ShaderDescriptor> descriptor) {
  return m_shader_descriptor_pool.get(
      m_shader_descriptor_pool.register_or_get(descriptor)
  );
}

bool ResourceManager::reload_shader(const ResourceDescriptorID &descriptor_id) {
  auto shader = get_by_descriptor_id<Shader>(descriptor_id);
  if (shader == nullptr) return false;
  return shader->recompile();
}

void ResourceManager::request_texture_2d_async(
    RendererBackend backend,
    const ResourceDescriptorID &descriptor_id
) {
  auto descriptor = get_descriptor_by_id<Texture2DDescriptor>(descriptor_id);
  if (descriptor == nullptr) {
    return;
  }

  auto &resource_pool = get_resource_pool_of<Texture2DDescriptor>();
  if (resource_pool.has_handle_by_id(descriptor_id)) {
    return;
  }

  if (!descriptor->image_load.has_value()) {
    resource_pool.register_or_get(backend, descriptor);
    return;
  }

  auto *jobs = JobSystem::get();
  if (jobs == nullptr) {
    resource_pool.register_or_get(backend, descriptor);
    return;
  }

  {
    std::lock_guard lock(m_async_resource_mutex);
    if (!m_pending_texture_2d_loads.insert(descriptor_id).second) {
      return;
    }
  }

  auto pending = std::make_shared<PendingTexture2DLoad>();
  JobHandle decode_job = jobs->submit(
      [descriptor, pending]() {
        pending->prepared = Texture2D::prepare_descriptor(descriptor);
      },
      JobQueue::Worker,
      JobPriority::Normal
  );

  const std::array<JobHandle, 1> dependencies = {decode_job};
  jobs->submit_after(
      dependencies,
      [this, backend, descriptor_id, descriptor, pending, decode_job]() mutable {
        auto clear_pending = make_scope_exit([this, descriptor_id]() {
          std::lock_guard lock(m_async_resource_mutex);
          m_pending_texture_2d_loads.erase(descriptor_id);
        });

        JobSystem::get()->wait(decode_job);
        descriptor->backend = backend;
        auto &pool = get_resource_pool_of<Texture2DDescriptor>();
        pool.register_or_get_with_factory(
            descriptor_id,
            [&descriptor, &pending](const ResourceHandle &handle) mutable {
              return Texture2D::from_prepared_descriptor(
                  handle,
                  descriptor,
                  std::move(pending->prepared)
              );
            }
        );
      },
      JobQueue::Main,
      JobPriority::Normal
  );
}

void ResourceManager::request_model_async(
    const ResourceDescriptorID &descriptor_id
) {
  auto descriptor = get_descriptor_by_id<ModelDescriptor>(descriptor_id);
  if (descriptor == nullptr) {
    return;
  }

  auto &resource_pool = get_resource_pool_of<ModelDescriptor>();
  if (resource_pool.has_handle_by_id(descriptor_id)) {
    return;
  }

  auto *jobs = JobSystem::get();
  if (jobs == nullptr) {
    resource_pool.register_or_get(RendererBackend::None, descriptor);
    return;
  }

  {
    std::lock_guard lock(m_async_resource_mutex);
    if (!m_pending_model_loads.insert(descriptor_id).second) {
      return;
    }
  }

  auto pending = std::make_shared<PendingModelLoad>();
  JobHandle import_job = jobs->submit(
      [descriptor, pending]() {
        pending->imported = import_model_file(
            path_manager()->resolve(descriptor->source_path),
            descriptor->import_settings
        );
      },
      JobQueue::Worker,
      JobPriority::Normal
  );

  const std::array<JobHandle, 1> dependencies = {import_job};
  jobs->submit_after(
      dependencies,
      [this, descriptor_id, descriptor, pending, import_job]() mutable {
        auto clear_pending = make_scope_exit([this, descriptor_id]() {
          std::lock_guard lock(m_async_resource_mutex);
          m_pending_model_loads.erase(descriptor_id);
        });

        JobSystem::get()->wait(import_job);
        auto &pool = get_resource_pool_of<ModelDescriptor>();
        pool.register_or_get_with_factory(
            descriptor_id,
            [&descriptor, &pending](const ResourceHandle &handle) mutable {
              return Model::from_imported_data(
                  handle,
                  descriptor,
                  std::move(pending->imported)
              );
            }
        );
      },
      JobQueue::Main,
      JobPriority::Normal
  );
}

std::vector<Ref<ShaderDescriptor>> ResourceManager::shader_descriptors() const {
  std::vector<Ref<ShaderDescriptor>> descriptors;
  descriptors.reserve(m_shader_descriptor_pool.slots.size());

  for (const auto &slot : m_shader_descriptor_pool.slots) {
    if (slot.descriptor != nullptr) {
      descriptors.push_back(slot.descriptor);
    }
  }

  return descriptors;
}

void ResourceManager::register_shaders(
    std::initializer_list<Ref<ShaderDescriptor>> descriptors
) {
  for (auto descriptor : descriptors) {
    register_shader(descriptor);
  }
}

Ref<ModelDescriptor>
ResourceManager::register_model(Ref<ModelDescriptor> descriptor) {
  return m_model_descriptor_pool.get(
      m_model_descriptor_pool.register_or_get(descriptor)
  );
}

Ref<SvgDescriptor> ResourceManager::register_svg(Ref<SvgDescriptor> descriptor) {
  return m_svg_descriptor_pool.get(
      m_svg_descriptor_pool.register_or_get(descriptor)
  );
}

void ResourceManager::register_models(
    std::initializer_list<Ref<ModelDescriptor>> models
) {
  for (auto &model : models) {
    register_model(model);
  }
}

void ResourceManager::register_fonts(
    std::initializer_list<Ref<FontDescriptor>> fonts
) {
  for (auto font : fonts) {
    register_font(font);
  }
}

void ResourceManager::register_svgs(
    std::initializer_list<Ref<SvgDescriptor>> svgs
) {
  for (auto svg : svgs) {
    register_svg(svg);
  }
}

Ref<AudioClipDescriptor>
ResourceManager::register_audio_clip(Ref<AudioClipDescriptor> descriptor) {
  return m_audio_clip_descriptor_pool.get(
      m_audio_clip_descriptor_pool.register_or_get(descriptor)
  );
}

Ref<TerrainRecipeDescriptor>
ResourceManager::register_terrain_recipe(Ref<TerrainRecipeDescriptor> recipe) {
  return m_terrain_recipe_descriptor_pool.get(
      m_terrain_recipe_descriptor_pool.register_or_get(recipe)
  );
}

Ref<FontDescriptor>
ResourceManager::register_font(Ref<FontDescriptor> descriptor) {
  return m_font_descriptor_pool.get(
      m_font_descriptor_pool.register_or_get(descriptor)
  );
}

Ref<MaterialDescriptor>
ResourceManager::register_material(Ref<MaterialDescriptor> material) {
  return m_material_descriptor_pool.get(
      m_material_descriptor_pool.register_or_get(material)
  );
}

void ResourceManager::register_materials(
    std::initializer_list<Ref<MaterialDescriptor>> materials
) {
  for (auto material : materials) {
    register_material(material);
  }
};

std::vector<Model *> ResourceManager::get_model_by_descriptor_ids(
    std::initializer_list<ResourceDescriptorID> ids
) {
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
