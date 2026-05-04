#pragma once

#include "framebuffer.hpp"
#include "render-graph-pass.hpp"
#include "render-graph-resource.hpp"
#include <unordered_map>
#include <vector>

namespace astralix {

class RenderGraph;

class PassBuilder;

class RenderGraphBuilder {
public:
  RenderGraphBuilder() = default;

  ImageHandle declare_image(
      const std::string &name, ImageDesc desc,
      RenderGraphResourceLifetime lifetime = RenderGraphResourceLifetime::Transient
  ) {
    RenderGraphResourceDescriptor resource_desc;
    resource_desc.type = RenderGraphResourceType::Image;
    resource_desc.name = name;
    resource_desc.lifetime = lifetime;

    ImageHandle handle{m_next_image_handle_id++};
    resource_desc.explicit_handle_id = handle.id;

    if (desc.debug_name.empty()) {
      desc.debug_name = name;
    }

    desc.persistent = lifetime == RenderGraphResourceLifetime::Persistent;
    resource_desc.spec = desc;

    m_resource_descs.push_back(resource_desc);
    m_image_resource_indices.emplace(
        handle.id, static_cast<uint32_t>(m_resource_descs.size() - 1)
    );
    return handle;
  }

  ImageHandle declare_window_relative_image(
      const std::string &name, uint32_t width, uint32_t height,
      ImageFormat format, ImageUsage usage, uint32_t samples = 1,
      RenderGraphResourceLifetime lifetime = RenderGraphResourceLifetime::Transient
  ) {
    ImageDesc desc;
    desc.debug_name = name;
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.samples = samples;
    desc.format = format;
    desc.usage = usage;
    desc.extent.mode = ImageExtentMode::WindowRelative;
    return declare_image(name, desc, lifetime);
  }

  BufferHandle declare_buffer(
      const std::string &name, BufferDesc desc,
      RenderGraphResourceLifetime lifetime = RenderGraphResourceLifetime::Transient
  ) {
    RenderGraphResourceDescriptor resource_desc;
    resource_desc.type = RenderGraphResourceType::Buffer;
    resource_desc.name = name;
    resource_desc.lifetime = lifetime;

    BufferHandle handle{m_next_buffer_handle_id++};
    resource_desc.explicit_handle_id = handle.id;

    if (desc.debug_name.empty()) {
      desc.debug_name = name;
    }

    desc.persistent = lifetime == RenderGraphResourceLifetime::Persistent;
    resource_desc.spec = desc;

    m_resource_descs.push_back(resource_desc);
    m_buffer_resource_indices.emplace(
        handle.id, static_cast<uint32_t>(m_resource_descs.size() - 1)
    );
    return handle;
  }

  uint32_t declare_texture_2d(
      const std::string &name, uint32_t width, uint32_t height,
      FramebufferTextureFormat format = FramebufferTextureFormat::RGBA8,
      uint32_t mip_levels = 1, uint32_t sample_count = 1
  ) {

    RenderGraphResourceDescriptor desc;
    desc.type = RenderGraphResourceType::Texture2D;
    desc.name = name;
    desc.lifetime = RenderGraphResourceLifetime::Transient;

    TextureSpec spec;
    spec.width = width;
    spec.height = height;
    spec.depth = 1;
    spec.format = format;
    spec.mip_levels = mip_levels;
    spec.sample_count = sample_count;
    desc.spec = spec;

    m_resource_descs.push_back(desc);
    return static_cast<uint32_t>(m_resource_descs.size() - 1);
  }

  uint32_t declare_storage_buffer(const std::string &name, uint32_t size, RenderGraphResourceLifetime lifetime = RenderGraphResourceLifetime::Transient) {

    RenderGraphResourceDescriptor desc;
    desc.type = RenderGraphResourceType::StorageBuffer;
    desc.name = name;
    desc.lifetime = lifetime;

    StorageBufferSpec spec;
    spec.size = size;
    desc.spec = spec;

    m_resource_descs.push_back(desc);
    return static_cast<uint32_t>(m_resource_descs.size() - 1);
  }

  uint32_t import_persistent_texture(const std::string &name, ResourceHandle handle) {
    RenderGraphResourceDescriptor desc;
    desc.type = RenderGraphResourceType::Texture2D;
    desc.name = name;
    desc.lifetime = RenderGraphResourceLifetime::Persistent;
    desc.external_resource = handle;

    m_resource_descs.push_back(desc);
    return static_cast<uint32_t>(m_resource_descs.size() - 1);
  }

  template <typename T>
  uint32_t declare_logical_buffer(const std::string &name) {
    RenderGraphResourceDescriptor desc;
    desc.type = RenderGraphResourceType::LogicalBuffer;
    desc.name = name;
    desc.lifetime = RenderGraphResourceLifetime::Transient;

    LogicalBufferSpec spec{
        .size_hint = sizeof(T),
        .constructor = [](void *mem) -> void * { return new (mem) T(); },
        .destructor = [](void *ptr) { static_cast<T *>(ptr)->~T(); }
    };

    desc.spec = spec;
    m_resource_descs.push_back(desc);

    return static_cast<uint32_t>(m_resource_descs.size() - 1);
  }

  PassBuilder
  add_pass(Scope<FramePass> pass, RenderGraphPassType type = RenderGraphPassType::Graphics);

  Scope<RenderGraph> build();

  std::vector<Scope<RenderGraphPass>> take_passes() {
    return std::move(m_passes);
  }

  uint32_t resolve_resource_index(ImageHandle handle) const {
    auto it = m_image_resource_indices.find(handle.id);
    ASTRA_ENSURE(
        it == m_image_resource_indices.end(),
        "Unknown image handle in render graph builder: ",
        handle.id
    );
    return it->second;
  }

  uint32_t resolve_resource_index(BufferHandle handle) const {
    auto it = m_buffer_resource_indices.find(handle.id);
    ASTRA_ENSURE(
        it == m_buffer_resource_indices.end(),
        "Unknown buffer handle in render graph builder: ",
        handle.id
    );
    return it->second;
  }

private:
  std::vector<RenderGraphResourceDescriptor> m_resource_descs;
  std::vector<Scope<RenderGraphPass>> m_passes;
  std::unordered_map<uint32_t, uint32_t> m_image_resource_indices;
  std::unordered_map<uint32_t, uint32_t> m_buffer_resource_indices;
  uint32_t m_next_image_handle_id = 1;
  uint32_t m_next_buffer_handle_id = 1;

  friend class RenderGraph;
};

class PassBuilder {
public:
  PassBuilder(RenderGraphBuilder *builder, RenderGraphPass *pass)
      : m_builder(builder), m_pass(pass) {
  }

  PassBuilder &read(uint32_t resource_index) {
    m_pass->read(resource_index);
    return *this;
  }

  PassBuilder &read(ImageHandle handle) {
    return read(m_builder->resolve_resource_index(handle));
  }

  PassBuilder &read(BufferHandle handle) {
    return read(m_builder->resolve_resource_index(handle));
  }

  PassBuilder &write(uint32_t resource_index) {
    m_pass->write(resource_index);
    return *this;
  }

  PassBuilder &write(ImageHandle handle) {
    return write(m_builder->resolve_resource_index(handle));
  }

  PassBuilder &write(BufferHandle handle) {
    return write(m_builder->resolve_resource_index(handle));
  }

  PassBuilder &read_write(uint32_t resource_index) {
    m_pass->read_write(resource_index);
    return *this;
  }

  PassBuilder &read_write(ImageHandle handle) {
    return read_write(m_builder->resolve_resource_index(handle));
  }

  PassBuilder &read_write(BufferHandle handle) {
    return read_write(m_builder->resolve_resource_index(handle));
  }

  PassBuilder &use_image(
      uint32_t resource_index, ImageAspect aspect, RenderUsage usage
  ) {
    m_pass->use_image(
        RenderImageSubresourceRef{
            .resource = RenderResourceRef{.resource_index = resource_index},
            .aspect = aspect,
        },
        usage
    );
    return *this;
  }

  PassBuilder &use_image(
      ImageHandle handle, ImageAspect aspect, RenderUsage usage
  ) {
    return use_image(m_builder->resolve_resource_index(handle), aspect, usage);
  }

  PassBuilder &use_image(
      BufferHandle handle, ImageAspect aspect, RenderUsage usage
  ) {
    return use_image(m_builder->resolve_resource_index(handle), aspect, usage);
  }

  PassBuilder &use_resource(
      RenderPassDependencyType type,
      std::string slot,
      ResourceDescriptorID descriptor_id
  ) {
    m_pass->add_asset_dependency(RenderPassDependencyDeclaration{
        .type = type,
        .slot = std::move(slot),
        .descriptor_id = std::move(descriptor_id),
    });
    return *this;
  }

  PassBuilder &use_shader(
      std::string slot, ResourceDescriptorID descriptor_id
  ) {
    return use_resource(
        RenderPassDependencyType::Shader,
        std::move(slot),
        std::move(descriptor_id)
    );
  }

  PassBuilder &use_texture_2d(
      std::string slot, ResourceDescriptorID descriptor_id
  ) {
    return use_resource(
        RenderPassDependencyType::Texture2D,
        std::move(slot),
        std::move(descriptor_id)
    );
  }

  PassBuilder &use_texture_3d(
      std::string slot, ResourceDescriptorID descriptor_id
  ) {
    return use_resource(
        RenderPassDependencyType::Texture3D,
        std::move(slot),
        std::move(descriptor_id)
    );
  }

  PassBuilder &use_material(
      std::string slot, ResourceDescriptorID descriptor_id
  ) {
    return use_resource(
        RenderPassDependencyType::Material,
        std::move(slot),
        std::move(descriptor_id)
    );
  }

  PassBuilder &use_model(
      std::string slot, ResourceDescriptorID descriptor_id
  ) {
    return use_resource(
        RenderPassDependencyType::Model,
        std::move(slot),
        std::move(descriptor_id)
    );
  }

  PassBuilder &use_font(
      std::string slot, ResourceDescriptorID descriptor_id
  ) {
    return use_resource(
        RenderPassDependencyType::Font,
        std::move(slot),
        std::move(descriptor_id)
    );
  }

  PassBuilder &use_svg(
      std::string slot, ResourceDescriptorID descriptor_id
  ) {
    return use_resource(
        RenderPassDependencyType::Svg,
        std::move(slot),
        std::move(descriptor_id)
    );
  }

  PassBuilder &use_audio_clip(
      std::string slot, ResourceDescriptorID descriptor_id
  ) {
    return use_resource(
        RenderPassDependencyType::AudioClip,
        std::move(slot),
        std::move(descriptor_id)
    );
  }

  PassBuilder &use_terrain_recipe(
      std::string slot, ResourceDescriptorID descriptor_id
  ) {
    return use_resource(
        RenderPassDependencyType::TerrainRecipe,
        std::move(slot),
        std::move(descriptor_id)
    );
  }

  PassBuilder &export_image(const RenderImageExport &export_request) {
    m_pass->export_image(export_request);
    return *this;
  }

  PassBuilder &present(uint32_t resource_index, ImageAspect aspect) {
    m_pass->present(
        RenderImageSubresourceRef{
            .resource = RenderResourceRef{.resource_index = resource_index},
            .aspect = aspect,
        }
    );
    return *this;
  }

  PassBuilder &present(ImageHandle handle, ImageAspect aspect) {
    return present(m_builder->resolve_resource_index(handle), aspect);
  }

  RenderGraphBuilder *end() { return m_builder; }

private:
  RenderGraphBuilder *m_builder;
  RenderGraphPass *m_pass;
};

inline PassBuilder RenderGraphBuilder::add_pass(Scope<FramePass> pass, RenderGraphPassType type) {
  auto graph_pass = create_scope<RenderGraphPass>(std::move(pass), type);
  RenderGraphPass *pass_ptr = graph_pass.get();
  m_passes.push_back(std::move(graph_pass));
  return PassBuilder(this, pass_ptr);
}

} // namespace astralix
