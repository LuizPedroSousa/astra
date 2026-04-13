#pragma once

#include "assert.hpp"
#include "framebuffer.hpp"
#include "guid.hpp"
#include "resources/texture.hpp"
#include "storage-buffer.hpp"
#include "systems/render-system/core/render-types.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>

namespace astralix {

enum class RenderGraphResourceType {
  Image,
  Buffer,
  Texture2D,
  Texture3D,
  StorageBuffer,
  LogicalBuffer
};

enum class RenderGraphResourceLifetime { Transient, Persistent };

struct RenderGraphImageResource {
  ImageDesc desc{};
  uint32_t generation = 1;

  [[nodiscard]] ImageExtent extent() const noexcept {
    return ImageExtent{
        .width = desc.width,
        .height = desc.height,
        .depth = desc.depth,
    };
  }

  void update_desc(const ImageDesc &next_desc) {
    if (desc.width != next_desc.width || desc.height != next_desc.height ||
        desc.depth != next_desc.depth || desc.mip_levels != next_desc.mip_levels ||
        desc.samples != next_desc.samples || desc.format != next_desc.format ||
        desc.usage != next_desc.usage) {
      ++generation;
    }
    desc = next_desc;
  }
};

struct TextureSpec {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 1;
  FramebufferTextureFormat format = FramebufferTextureFormat::RGBA8;
  uint32_t mip_levels = 1;
  uint32_t sample_count = 1;
};

struct StorageBufferSpec {
  uint32_t size = 0;
};

struct LogicalBufferSpec {
  uint32_t size_hint = 0;

  std::function<void *(void *)> constructor;
  std::function<void(void *)> destructor;
};

struct RenderGraphResourceDescriptor {
  RenderGraphResourceType type;
  std::string name;
  RenderGraphResourceLifetime lifetime;
  uint32_t explicit_handle_id = 0;

  std::variant<ImageDesc, BufferDesc, TextureSpec, LogicalBufferSpec,
               StorageBufferSpec>
      spec;

  std::variant<std::monostate, std::shared_ptr<RenderGraphImageResource>,
               StorageBuffer *, ResourceHandle, void *>
      external_resource;
};

struct RenderGraphResource {
  RenderGraphResourceDescriptor desc;

  int32_t first_write_pass = -1;
  int32_t last_read_pass = -1;
  bool is_written = false;
  bool is_read = false;

  int32_t alias_group = -1;

  std::variant<std::monostate, std::shared_ptr<RenderGraphImageResource>,
               StorageBuffer *, ResourceHandle, void *>
      content;

  explicit RenderGraphResource(const RenderGraphResourceDescriptor &d)
      : desc(d), content(d.external_resource) {}

  bool is_transient() const {
    return desc.lifetime == RenderGraphResourceLifetime::Transient;
  }

  bool is_persistent() const {
    return desc.lifetime == RenderGraphResourceLifetime::Persistent;
  }

  void *get_logical_buffer() const {
    if (auto *data = std::get_if<void *>(&content)) {
      return *data;
    }

    return nullptr;
  }

  StorageBuffer *get_storage_buffer() const {
    if (auto *data = std::get_if<StorageBuffer *>(&content)) {
      return *data;
    }

    ASTRA_EXCEPTION("Resource content is not a StorageBuffer");
  }

  std::shared_ptr<RenderGraphImageResource> get_graph_image() const {
    if (auto *image = std::get_if<std::shared_ptr<RenderGraphImageResource>>(
            &content)) {
      return *image;
    }

    return nullptr;
  }

  ResourceHandle get_texture_handle() const {
    if (auto *handle = std::get_if<ResourceHandle>(&content)) {
      return *handle;
    }
    return ResourceHandle{};
  }

  void set_content(ResourceHandle handle) { content = handle; }
  void set_content(std::shared_ptr<RenderGraphImageResource> image) {
    content = std::move(image);
  }

  template <typename T> void set_content(T value) { content = value; }
};

} // namespace astralix
