#pragma once

#include "resources/descriptors/resource-descriptor.hpp"
#include "resources/texture.hpp"
#include <optional>
#include <unordered_map>

namespace astralix {
enum class TextureParameter;
enum class TextureValue;
enum class TextureFormat;
class TextureConfig;

struct ImageLoad {
  Ref<Path> path;
  bool flip_image_on_loading = false;
};

struct Texture2DDescriptor {
public:
  Texture2DDescriptor(const ResourceDescriptorID &id)
      : RESOURCE_DESCRIPTOR_INIT() {}

  Texture2DDescriptor(
      const ResourceDescriptorID &id, Ref<Path> path,
      bool flip_image_on_loading = false,
      std::unordered_map<TextureParameter, TextureValue> parameters = {});

  Texture2DDescriptor(
      const ResourceDescriptorID &id, uint32_t width, uint32_t height,
      std::unordered_map<TextureParameter, TextureValue> parameters,
      bool bitmap, TextureFormat format, unsigned char *buffer);

  static Ref<Texture2DDescriptor>
  create(const ResourceDescriptorID &id, Ref<Path> path,
         bool flip_image_on_loading = false,
         std::unordered_map<TextureParameter, TextureValue> parameters = {});

  static Ref<Texture2DDescriptor> create(const ResourceDescriptorID &id,
                                         TextureConfig config);

  RESOURCE_DESCRIPTOR_PARAMS;

  std::unordered_map<TextureParameter, TextureValue> parameters;

  std::optional<ImageLoad> image_load;

  uint32_t width = 0;
  uint32_t height = 0;
  bool bitmap = true;
  TextureFormat format;
  uint32_t slot = 0;
  unsigned char *buffer = nullptr;
  RendererBackend backend = RendererBackend::None;
};

struct Texture3DDescriptor {
public:
  Texture3DDescriptor(const ResourceDescriptorID &id,
                      const std::vector<Ref<Path>> &face_paths)
      : RESOURCE_DESCRIPTOR_INIT(), face_paths(face_paths) {}

  static Ref<Texture3DDescriptor>
  create(const ResourceDescriptorID &id,
         const std::vector<Ref<Path>> &face_paths);

  RESOURCE_DESCRIPTOR_PARAMS;

  std::vector<Ref<Path>> face_paths;

  uint32_t slot = 0;
  RendererBackend backend = RendererBackend::None;
};
} // namespace astralix
