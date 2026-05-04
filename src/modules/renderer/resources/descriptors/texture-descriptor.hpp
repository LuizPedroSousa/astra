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
      std::unordered_map<TextureParameter, TextureValue> parameters = {}
  );

  Texture2DDescriptor(
      const ResourceDescriptorID &id, uint32_t width, uint32_t height,
      std::unordered_map<TextureParameter, TextureValue> parameters,
      bool bitmap, TextureFormat format, unsigned char *buffer
  );

  static Ref<Texture2DDescriptor>
  create(const ResourceDescriptorID &id, Ref<Path> path, bool flip_image_on_loading = false, std::unordered_map<TextureParameter, TextureValue> parameters = {});

  static Ref<Texture2DDescriptor> create(const ResourceDescriptorID &id, TextureConfig config);

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
  Texture3DDescriptor(const ResourceDescriptorID &id, const std::vector<Ref<Path>> &face_paths)
      : RESOURCE_DESCRIPTOR_INIT(), face_paths(face_paths) {}

  Texture3DDescriptor(
      const ResourceDescriptorID &id,
      Ref<Path> equirectangular_path,
      uint32_t face_resolution
  )
      : RESOURCE_DESCRIPTOR_INIT(),
        equirectangular_path(std::move(equirectangular_path)),
        equirectangular_face_resolution(face_resolution),
        is_equirectangular(true) {}

  Texture3DDescriptor(
      const ResourceDescriptorID &id,
      uint32_t face_width,
      uint32_t face_height,
      const std::vector<const unsigned char *> &face_buffers
  )
      : RESOURCE_DESCRIPTOR_INIT(),
        buffer_face_width(face_width),
        buffer_face_height(face_height),
        buffer_faces(face_buffers),
        is_from_buffer(true) {}

  Texture3DDescriptor(
      const ResourceDescriptorID &id,
      uint32_t face_width,
      uint32_t face_height,
      std::vector<std::vector<float>> float_face_data
  )
      : RESOURCE_DESCRIPTOR_INIT(),
        buffer_face_width(face_width),
        buffer_face_height(face_height),
        float_buffer_faces(std::move(float_face_data)),
        is_from_float_buffer(true) {}

  static Ref<Texture3DDescriptor>
  create(const ResourceDescriptorID &id, const std::vector<Ref<Path>> &face_paths);

  static Ref<Texture3DDescriptor>
  create_from_equirectangular(const ResourceDescriptorID &id, Ref<Path> equirectangular_path, uint32_t face_resolution = 1024);

  static Ref<Texture3DDescriptor>
  create_from_buffer(
      const ResourceDescriptorID &id,
      uint32_t face_width,
      uint32_t face_height,
      const std::vector<const unsigned char *> &face_buffers
  );

  static Ref<Texture3DDescriptor>
  create_from_float_buffer(
      const ResourceDescriptorID &id,
      uint32_t face_width,
      uint32_t face_height,
      std::vector<std::vector<float>> float_face_data
  );

  RESOURCE_DESCRIPTOR_PARAMS;

  std::vector<Ref<Path>> face_paths;
  Ref<Path> equirectangular_path;
  uint32_t equirectangular_face_resolution = 1024;
  bool is_equirectangular = false;

  uint32_t buffer_face_width = 0;
  uint32_t buffer_face_height = 0;
  std::vector<const unsigned char *> buffer_faces;
  bool is_from_buffer = false;

  std::vector<std::vector<float>> float_buffer_faces;
  bool is_from_float_buffer = false;

  uint32_t slot = 0;
  RendererBackend backend = RendererBackend::None;
};
} // namespace astralix
