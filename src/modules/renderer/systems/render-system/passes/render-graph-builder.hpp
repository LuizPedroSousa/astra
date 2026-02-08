#pragma once

#include "render-graph-pass.hpp"
#include "render-graph-resource.hpp"
#include <vector>

namespace astralix {

class RenderGraph;

class PassBuilder;

class RenderGraphBuilder {
public:
  RenderGraphBuilder() = default;

  uint32_t declare_texture_2d(
      const std::string &name, uint32_t width, uint32_t height,
      FramebufferTextureFormat format = FramebufferTextureFormat::RGBA8,
      uint32_t mip_levels = 1, uint32_t sample_count = 1) {

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

  uint32_t declare_framebuffer(
      const std::string &name, uint32_t width, uint32_t height,
      FramebufferTextureFormat format = FramebufferTextureFormat::RGBA8,
      uint32_t sample_count = 1,
      RenderGraphResourceLifetime lifetime =
          RenderGraphResourceLifetime::Transient) {

    RenderGraphResourceDescriptor desc;
    desc.type = RenderGraphResourceType::Framebuffer;
    desc.name = name;
    desc.lifetime = lifetime;

    TextureSpec spec;
    spec.width = width;
    spec.height = height;
    spec.format = format;
    spec.sample_count = sample_count;
    desc.spec = spec;

    m_resource_descs.push_back(desc);
    return static_cast<uint32_t>(m_resource_descs.size() - 1);
  }
  uint32_t declare_storage_buffer(const std::string &name, uint32_t size,
                                  RenderGraphResourceLifetime lifetime =
                                      RenderGraphResourceLifetime::Transient) {

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

  uint32_t import_persistent_framebuffer(const std::string &name,
                                         Framebuffer *framebuffer) {
    RenderGraphResourceDescriptor desc;
    desc.type = RenderGraphResourceType::Framebuffer;
    desc.name = name;
    desc.lifetime = RenderGraphResourceLifetime::Persistent;
    desc.external_resource = framebuffer;

    m_resource_descs.push_back(desc);
    return static_cast<uint32_t>(m_resource_descs.size() - 1);
  }

  uint32_t import_persistent_texture(const std::string &name,
                                     ResourceHandle handle) {
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
        .destructor = [](void *ptr) { static_cast<T *>(ptr)->~T(); }};

    desc.spec = spec;
    m_resource_descs.push_back(desc);

    return static_cast<uint32_t>(m_resource_descs.size() - 1);
  }

  PassBuilder
  add_pass(Scope<RenderPass> pass,
           RenderGraphPassType type = RenderGraphPassType::Graphics);

  Scope<RenderGraph> build();

private:
  std::vector<RenderGraphResourceDescriptor> m_resource_descs;
  std::vector<Scope<RenderGraphPass>> m_passes;

  friend class RenderGraph;
};

class PassBuilder {
public:
  PassBuilder(RenderGraphBuilder *builder, RenderGraphPass *pass)
      : m_builder(builder), m_pass(pass) {}

  PassBuilder &read(uint32_t resource_index) {
    m_pass->read(resource_index);
    return *this;
  }

  PassBuilder &write(uint32_t resource_index) {
    m_pass->write(resource_index);
    return *this;
  }

  PassBuilder &read_write(uint32_t resource_index) {
    m_pass->read_write(resource_index);
    return *this;
  }

  RenderGraphBuilder *end() { return m_builder; }

private:
  RenderGraphBuilder *m_builder;
  RenderGraphPass *m_pass;
};

inline PassBuilder RenderGraphBuilder::add_pass(Scope<RenderPass> pass,
                                                RenderGraphPassType type) {
  auto graph_pass = create_scope<RenderGraphPass>(std::move(pass), type);
  RenderGraphPass *pass_ptr = graph_pass.get();
  m_passes.push_back(std::move(graph_pass));
  return PassBuilder(this, pass_ptr);
}

} // namespace astralix
