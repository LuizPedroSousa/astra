#pragma once

#include "base.hpp"
#include "guid.hpp"
#include "path.hpp"
#include "renderer-api.hpp"
#include "resource.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "vector"
#include <optional>
#include <unordered_map>

namespace astralix {

struct Image {
  int width;
  int height;
  int nr_channels;
  unsigned char *data;
};

enum class TextureParameter {
  WrapS = 0,
  WrapT = 1,
  MagFilter = 2,
  MinFilter = 3
};

enum class TextureValue {
  Repeat = 0,
  ClampToEdge = 1,
  ClampToBorder = 2,
  Linear = 3,
  Nearest = 4,
  LinearMipMap = 5
};

enum class TextureFormat { Red = 0, RGB = 1, RGBA = 2 };

class Texture2DDescriptor;
class Texture3DDescriptor;

class Texture : public Resource {
public:
  Texture(const ResourceHandle &resource_id) : Resource(resource_id) {};
  virtual void bind() const = 0;
  virtual void active(uint32_t slot) const = 0;
  virtual uint32_t renderer_id() const = 0;
  virtual uint32_t width() const = 0;
  virtual uint32_t height() const = 0;

  int get_slot() { return m_slot; }
  void set_slot(int slot) { m_slot = slot; }

protected:
  static Image load_image(Ref<Path> path, bool flip_image_on_loading);
  static void free_image(u_char *data);
  static int get_image_format(int nr_channels);
  uint32_t m_slot;
};

struct LoadImageConfig {
  Ref<Path> path;
  bool flip_image_on_loading = false;
};

struct TextureConfig {
  std::optional<LoadImageConfig> load_image;

  uint32_t width = 0;
  uint32_t height = 0;
  bool bitmap = true;
  TextureFormat format = TextureFormat::Red;

  std::unordered_map<TextureParameter, TextureValue> parameters;

  unsigned char *buffer = nullptr;
};

class Texture2D : public Texture {
public:
  static Ref<Texture2DDescriptor>
  create(const ResourceDescriptorID &id, Ref<Path> path,
         const bool flip_image_on_loading = false,
         std::unordered_map<TextureParameter, TextureValue> parameters = {
             {TextureParameter::WrapS, TextureValue::Linear},
             {TextureParameter::WrapT, TextureValue::Linear},
             {TextureParameter::MagFilter, TextureValue::Nearest},
             {TextureParameter::MinFilter, TextureValue::Nearest}});

  static Ref<Texture2DDescriptor> create(const ResourceDescriptorID &id,
                                         TextureConfig config);

  static Ref<Texture2DDescriptor> define(const ResourceDescriptorID &id,
                                         TextureConfig config);

  static Ref<Texture2D> from_descriptor(const ResourceHandle &id,
                                        Ref<Texture2DDescriptor> descriptor);

  Texture2D(const ResourceHandle &id) : Texture(id) {};
};

class Texture3D : public Texture {
public:
  static Ref<Texture3DDescriptor>
  create(const ResourceDescriptorID &id,
         const std::vector<Ref<Path>> &faces_path);

  static Ref<Texture3DDescriptor>
  define(const ResourceDescriptorID &id,
         const std::vector<Ref<Path>> &faces_path);

  static Ref<Texture3D> from_descriptor(const ResourceHandle &id,
                                        Ref<Texture3DDescriptor> descriptor);

  Texture3D(const ResourceHandle &resource_id) : Texture(resource_id) {};
};

} // namespace astralix
