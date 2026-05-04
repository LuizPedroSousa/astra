#pragma once
#include "base-manager.hpp"
#include "base.hpp"
#include "guid.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/audio-clip-descriptor.hpp"
#include "resources/descriptors/terrain-recipe-descriptor.hpp"
#include "resources/descriptors/font-descriptor.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/descriptors/svg-descriptor.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "resources/font.hpp"
#include "resources/material.hpp"
#include "resources/model.hpp"
#include "resources/shader.hpp"
#include "resources/svg.hpp"
#include "trace.hpp"
#include <initializer_list>
#include <mutex>
#include "unordered_map"
#include <unordered_map>
#include <unordered_set>

namespace astralix {

#define RESOURCE_MANAGER_SUGGESTION_NAME "ResourceManager"

class ResourceManager : public BaseManager<ResourceManager> {
public:
  template <typename T, typename D>
  struct ResourcePool {
    struct Slot {
      Ref<T> resource;
      uint32_t generation = 0;
    };

    std::vector<Slot> slots;
    std::vector<uint32_t> freelist;
    std::unordered_map<ResourceDescriptorID, ResourceHandle> descriptor_to_id;

    ResourceHandle find_handle_strict_by_id(
        const ResourceDescriptorID &desc_id
    ) const {
      auto it = descriptor_to_id.find(desc_id);

      ASTRA_ENSURE_WITH_SUGGESTIONS(
          it == descriptor_to_id.end(), descriptor_to_id, desc_id, RESOURCE_MANAGER_SUGGESTION_NAME, type_name<T>()
      );

      return it->second;
    }

    bool has_handle_by_id(const ResourceDescriptorID &desc_id) const {
      auto it = descriptor_to_id.find(desc_id);

      return it != descriptor_to_id.end();
    }

    template <typename Factory>
    ResourceHandle register_or_get_with_factory(
        const ResourceDescriptorID &desc_id,
        Factory &&factory
    ) {
      auto it = descriptor_to_id.find(desc_id);

      if (it != descriptor_to_id.end()) {
        return it->second;
      }

      ASTRA_PROFILE_DYN(desc_id.c_str(), desc_id.size());

      uint32_t slot_idx;
      uint32_t gen;

      if (!freelist.empty()) {
        slot_idx = freelist.back();
        freelist.pop_back();
        gen = slots[slot_idx].generation + 1;
      } else {
        slot_idx = slots.size();
        gen = 1;
        slots.emplace_back();
      }

      Handle handle{slot_idx, gen};
      Ref<T> resource = std::forward<Factory>(factory)(handle);

      slots[slot_idx] = {std::move(resource), gen};
      descriptor_to_id.emplace(desc_id, handle);

      return handle;
    }

    ResourceHandle register_or_get(RendererBackend backend, Ref<D> desc) {
      ResourceDescriptorID desc_id = desc->id;

      if constexpr (std::is_same_v<D, ShaderDescriptor> || std::is_same_v<D, Texture2DDescriptor> || std::is_same_v<D, Texture3DDescriptor> || std::is_same_v<D, FontDescriptor>) {
        desc->backend = backend;
      }

      return register_or_get_with_factory(
          desc_id,
          [&](const ResourceHandle &handle) {
            return T::from_descriptor(handle, desc);
          }
      );
    }

    Ref<T> get(Handle handle) const {
      if (!handle.is_valid() || handle.index >= slots.size()) {
        return nullptr;
      }

      const auto &slot = slots[handle.index];
      if (slot.generation != handle.generation) {
        return nullptr;
      }

      return slot.resource;
    }

    void release(const ResourceDescriptorID &desc_id) {
      auto it = descriptor_to_id.find(desc_id);
      if (it == descriptor_to_id.end())
        return;

      Handle handle = it->second;
      descriptor_to_id.erase(it);

      if (handle.index < slots.size()) {
        auto &slot = slots[handle.index];

        if (slot.generation == handle.generation) {
          slot.resource.reset();
          slot.generation++;
          freelist.push_back(handle.index);
        }
      }
    }
  };

  template <typename T>
  struct ResourceDescriptorPool {
    struct Slot {
      Ref<T> descriptor;
      uint32_t generation = 0;
    };

    std::vector<Slot> slots;
    std::vector<uint32_t> freelist;
    std::unordered_map<ResourceDescriptorID, Handle> descriptor_to_id;

    bool has_handle_by_id(const ResourceDescriptorID &desc_id) const {
      auto it = descriptor_to_id.find(desc_id);

      return it != descriptor_to_id.end();
    }

    Handle find_handle_strict_by_id(
        const ResourceDescriptorID &desc_id
    ) const {
      auto it = descriptor_to_id.find(desc_id);

      ASTRA_ENSURE_WITH_SUGGESTIONS(
          it == descriptor_to_id.end(), descriptor_to_id, desc_id, RESOURCE_MANAGER_SUGGESTION_NAME, type_name<T>()
      );

      return it->second;
    }

    Handle register_or_get(Ref<T> desc) {
      ResourceDescriptorID desc_id = desc->id;

      auto it = descriptor_to_id.find(desc_id);

      if (it != descriptor_to_id.end()) {
        return it->second;
      }

      uint32_t slot_idx;
      uint32_t gen;

      if (!freelist.empty()) {
        slot_idx = freelist.back();
        freelist.pop_back();
        gen = slots[slot_idx].generation + 1;
      } else {
        slot_idx = slots.size();
        gen = 1;
        slots.emplace_back();
      }

      slots[slot_idx] = {std::move(desc), gen};

      Handle handle{slot_idx, gen};
      descriptor_to_id.emplace(desc_id, handle);

      return handle;
    }

    Ref<T> get(Handle handle) const {
      if (!handle.is_valid() || handle.index >= slots.size()) {
        return nullptr;
      }

      const auto &slot = slots[handle.index];
      if (slot.generation != handle.generation) {
        return nullptr;
      }

      return slot.descriptor;
    }

    void release(const ResourceDescriptorID &desc_id) {
      auto it = descriptor_to_id.find(desc_id);
      if (it == descriptor_to_id.end())
        return;

      Handle handle = it->second;
      descriptor_to_id.erase(it);

      if (handle.index < slots.size()) {
        auto &slot = slots[handle.index];
        if (slot.generation == handle.generation) {
          slot.descriptor.reset();
          slot.generation++;
          freelist.push_back(handle.index);
        }
      }
    }
  };

  Ref<Texture2DDescriptor> register_texture(Ref<Texture2DDescriptor> texture);
  Ref<Texture3DDescriptor> register_texture(Ref<Texture3DDescriptor> texture);
  Ref<ShaderDescriptor> register_shader(Ref<ShaderDescriptor> shader);
  Ref<MaterialDescriptor> register_material(Ref<MaterialDescriptor> material);
  Ref<FontDescriptor> register_font(Ref<FontDescriptor> font);
  Ref<ModelDescriptor> register_model(Ref<ModelDescriptor> model);
  Ref<SvgDescriptor> register_svg(Ref<SvgDescriptor> svg);
  Ref<AudioClipDescriptor> register_audio_clip(Ref<AudioClipDescriptor> clip);
  Ref<TerrainRecipeDescriptor> register_terrain_recipe(Ref<TerrainRecipeDescriptor> recipe);
  std::vector<Ref<ShaderDescriptor>> shader_descriptors() const;
  bool reload_shader(const ResourceDescriptorID &descriptor_id);
  void request_texture_2d_async(
      RendererBackend backend,
      const ResourceDescriptorID &descriptor_id
  );
  void request_model_async(const ResourceDescriptorID &descriptor_id);
  void register_models(std::initializer_list<Ref<ModelDescriptor>> models);

  void
  register_textures(std::initializer_list<Ref<Texture2DDescriptor>> textures);

  void
  register_textures(std::initializer_list<Ref<Texture3DDescriptor>> textures);
  void
  register_materials(std::initializer_list<Ref<MaterialDescriptor>> materials);

  void register_shaders(std::initializer_list<Ref<ShaderDescriptor>> shaders);

  void register_fonts(std::initializer_list<Ref<FontDescriptor>> fonts);
  void register_svgs(std::initializer_list<Ref<SvgDescriptor>> svgs);

  template <class T>
  Ref<T> get_by_descriptor_id(const ResourceDescriptorID &id) {
    auto &pool = get_pool_for<T>();

    if (pool.slots.empty() || !pool.has_handle_by_id(id)) {
      return nullptr;
    }

    return pool.get(pool.find_handle_strict_by_id(id));
  }

  template <class T>
  Ref<T> find_strict_by_descriptor_id(const ResourceDescriptorID &id) {
    auto &pool = get_pool_for<T>();

    if (pool.slots.empty()) {
      return nullptr;
    }

    return pool.get(pool.find_handle_strict_by_id(id));
  }

  template <class T>
  void ensure_exists_by_descriptor_id(const ResourceDescriptorID &id) {
    auto &pool = get_pool_for<T>();

    pool.find_handle_strict_by_id(id);
  }

  template <class Descriptor>
  void release_by_descriptor_id(const ResourceDescriptorID &id) {
    auto &resource_pool = get_resource_pool_of<Descriptor>();
    resource_pool.release(id);

    auto &descriptor_pool = get_pool_for<Descriptor>();
    descriptor_pool.release(id);
  }

  template <class Descriptor>
  void release_descriptor_by_id(const ResourceDescriptorID &id) {
    auto &descriptor_pool = get_pool_for<Descriptor>();
    descriptor_pool.release(id);
  }

  template <class Descriptor>
  Ref<Descriptor>
  get_descriptor_by_id(const ResourceDescriptorID &id) {
    auto &pool = get_pool_for<Descriptor>();

    if (pool.slots.empty() || !pool.has_handle_by_id(id)) {
      return nullptr;
    }

    return pool.get(pool.find_handle_strict_by_id(id));
  }

  template <class Descriptor>
  void load_from_descriptors_by_ids(
      RendererBackend backend,
      std::initializer_list<ResourceDescriptorID> ids
  ) {
    auto &descriptor_pool = get_pool_for<Descriptor>();
    auto &resource_pool = get_resource_pool_of<Descriptor>();

    for (auto id : ids) {
      auto descriptor =
          descriptor_pool.get(descriptor_pool.find_handle_strict_by_id(id));

      resource_pool.register_or_get(backend, descriptor);
    }
  }

  template <class Descriptor>
  void load_from_descriptor(RendererBackend backend) {
    ASTRA_PROFILE_DYN(type_name<Descriptor>().data(), type_name<Descriptor>().size());
    auto &descriptor_pool = get_pool_for<Descriptor>();
    auto &resource_pool = get_resource_pool_of<Descriptor>();

    for (auto slot : descriptor_pool.slots) {
      resource_pool.register_or_get(backend, slot.descriptor);
    }
  }

  template <class... Descriptors>
  void load_from_descriptors(RendererBackend backend) {
    ASTRA_PROFILE_N("load_from_descriptors");
    (load_from_descriptor<Descriptors>(backend), ...);
  }

  std::vector<Model *>
  get_model_by_descriptor_ids(std::initializer_list<ResourceDescriptorID> ids);

  int texture_2d_slot() {
    // return m_texture_2d_pool.slots.size() > 0
    //            ? m_texture_2d_pool.slots[m_texture_2d_pool.slots.size() - 1]
    //                  .resource->id()
    //                  .index
    //            : 0;

    return m_texture_2d_pool.slots.size();
  };
  int texture_3d_slot() { return m_texture_3d_pool.slots.size(); };

  size_t texture_count() const {
    return (m_texture_2d_pool.slots.size() - m_texture_2d_pool.freelist.size()) +
           (m_texture_3d_pool.slots.size() - m_texture_3d_pool.freelist.size());
  }
  size_t shader_count() const {
    return m_shader_pool.slots.size() - m_shader_pool.freelist.size();
  }
  size_t material_count() const {
    return m_material_pool.slots.size() - m_material_pool.freelist.size();
  }
  size_t model_count() const {
    return m_model_pool.slots.size() - m_model_pool.freelist.size();
  }

  ResourceManager() = default;

private:
  ResourceDescriptorPool<Texture2DDescriptor> m_texture_2d_descriptor_pool;
  ResourceDescriptorPool<Texture3DDescriptor> m_texture_3d_descriptor_pool;
  ResourceDescriptorPool<ShaderDescriptor> m_shader_descriptor_pool;
  ResourceDescriptorPool<ModelDescriptor> m_model_descriptor_pool;
  ResourceDescriptorPool<MaterialDescriptor> m_material_descriptor_pool;
  ResourceDescriptorPool<FontDescriptor> m_font_descriptor_pool;
  ResourceDescriptorPool<SvgDescriptor> m_svg_descriptor_pool;
  ResourceDescriptorPool<AudioClipDescriptor> m_audio_clip_descriptor_pool;
  ResourceDescriptorPool<TerrainRecipeDescriptor> m_terrain_recipe_descriptor_pool;

  ResourcePool<Texture2D, Texture2DDescriptor> m_texture_2d_pool;
  ResourcePool<Texture3D, Texture3DDescriptor> m_texture_3d_pool;
  ResourcePool<Shader, ShaderDescriptor> m_shader_pool;
  ResourcePool<Model, ModelDescriptor> m_model_pool;
  ResourcePool<Material, MaterialDescriptor> m_material_pool;
  ResourcePool<Font, FontDescriptor> m_font_pool;
  ResourcePool<Svg, SvgDescriptor> m_svg_pool;
  mutable std::mutex m_async_resource_mutex;
  std::unordered_set<ResourceDescriptorID> m_pending_texture_2d_loads;
  std::unordered_set<ResourceDescriptorID> m_pending_model_loads;

  template <class T>
  auto &get_pool_for();
  template <class T>
  auto &get_resource_pool_of();

  template <class T>
  static std::string_view type_name();
};

#define RESOURCE_POOL_LIST(MAP)                                                \
  MAP(MaterialDescriptor, m_material_descriptor_pool)                          \
  MAP(Texture2DDescriptor, m_texture_2d_descriptor_pool)                       \
  MAP(Texture3DDescriptor, m_texture_3d_descriptor_pool)                       \
  MAP(ShaderDescriptor, m_shader_descriptor_pool)                              \
  MAP(ModelDescriptor, m_model_descriptor_pool)                                \
  MAP(FontDescriptor, m_font_descriptor_pool)                                  \
  MAP(SvgDescriptor, m_svg_descriptor_pool)                                    \
  MAP(AudioClipDescriptor, m_audio_clip_descriptor_pool)                       \
  MAP(TerrainRecipeDescriptor, m_terrain_recipe_descriptor_pool)               \
                                                                               \
  MAP(Material, m_material_pool)                                               \
  MAP(Texture2D, m_texture_2d_pool)                                            \
  MAP(Texture3D, m_texture_3d_pool)                                            \
  MAP(Shader, m_shader_pool)                                                   \
  MAP(Model, m_model_pool)                                                     \
  MAP(Font, m_font_pool)                                                       \
  MAP(Svg, m_svg_pool)

#define RESOURCE_TYPENAME_LIST(MAP)                                            \
  MAP(MaterialDescriptor)                                                      \
  MAP(Texture2DDescriptor)                                                     \
  MAP(Texture3DDescriptor)                                                     \
  MAP(ShaderDescriptor)                                                        \
  MAP(ModelDescriptor)                                                         \
  MAP(FontDescriptor)                                                          \
  MAP(SvgDescriptor)                                                           \
  MAP(AudioClipDescriptor)                                                     \
  MAP(TerrainRecipeDescriptor)                                                 \
  MAP(Material)                                                                \
  MAP(Texture2D)                                                               \
  MAP(Texture3D)                                                               \
  MAP(Shader)                                                                  \
  MAP(Model)                                                                   \
  MAP(Font)                                                                    \
  MAP(Svg)

#define DESCRIPTOR_TO_RESOURCE_POOL_LIST(MAP) \
  MAP(MaterialDescriptor, m_material_pool)    \
  MAP(Texture2DDescriptor, m_texture_2d_pool) \
  MAP(Texture3DDescriptor, m_texture_3d_pool) \
  MAP(ShaderDescriptor, m_shader_pool)        \
  MAP(ModelDescriptor, m_model_pool)          \
  MAP(FontDescriptor, m_font_pool)            \
  MAP(SvgDescriptor, m_svg_pool)

#define MAP_TYPE_WITH_POOL(type, pool)                 \
  template <>                                          \
  inline auto &ResourceManager::get_pool_for<type>() { \
    return pool;                                       \
  }

#define MAP_DESCRIPTOR_POOL_WITH_RESOURCE_POOL(type, pool)     \
  template <>                                                  \
  inline auto &ResourceManager::get_resource_pool_of<type>() { \
    return pool;                                               \
  }

#define MAP_TYPE_WITH_TYPENAME(type)                           \
  template <>                                                  \
  inline std::string_view ResourceManager::type_name<type>() { \
    return #type;                                              \
  }

RESOURCE_POOL_LIST(MAP_TYPE_WITH_POOL)
DESCRIPTOR_TO_RESOURCE_POOL_LIST(MAP_DESCRIPTOR_POOL_WITH_RESOURCE_POOL)
RESOURCE_TYPENAME_LIST(MAP_TYPE_WITH_TYPENAME)

#undef MAP_TYPE_WITH_TYPENAME
#undef MAP_TYPE_WITH_POOL
#undef MAP_DESCRIPTOR_POOL_WITH_RESOURCE_POOL
#undef RESOURCE_POOL_LIST
#undef RESOURCE_TYPENAME_LIST
#undef DESCRIPTOR_TO_RESOURCE_POOL_LIST
#undef RESOURCE_MANAGER_SUGGESTION_NAME

inline Ref<ResourceManager> resource_manager() {
  return ResourceManager::get();
}
} // namespace astralix
