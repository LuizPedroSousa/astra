#include "resources/descriptors/texture-descriptor.hpp"
#include "base.hpp"
#include "resources/descriptors/resource-descriptor.hpp"
#include "resources/texture.hpp"

namespace astralix {

Texture2DDescriptor::Texture2DDescriptor(
    const ResourceDescriptorID &id, Ref<Path> path, bool flip_image_on_loading,
    std::unordered_map<TextureParameter, TextureValue> parameters)
    : RESOURCE_DESCRIPTOR_INIT(), parameters(std::move(parameters)),
      image_load(ImageLoad{std::move(path), flip_image_on_loading}),
      format(TextureFormat::Red) {}

Texture2DDescriptor::Texture2DDescriptor(
    const ResourceDescriptorID &id, uint32_t width, uint32_t height,
    std::unordered_map<TextureParameter, TextureValue> parameters, bool bitmap,
    TextureFormat format, unsigned char *buffer)
    : RESOURCE_DESCRIPTOR_INIT(), parameters(std::move(parameters)),
      width(width), height(height), buffer(buffer), bitmap(bitmap),
      format(format) {}

Ref<Texture2DDescriptor>
Texture2DDescriptor::create(const ResourceDescriptorID &id,
                            TextureConfig config) {
  return create_ref<Texture2DDescriptor>(id, config.width, config.height,
                                         config.parameters, config.bitmap,
                                         config.format, config.buffer);
};

Ref<Texture2DDescriptor> Texture2DDescriptor::create(
    const ResourceDescriptorID &id, Ref<Path> path,
    const bool flip_image_on_loading,
    std::unordered_map<TextureParameter, TextureValue> parameters) {
  if (parameters.empty()) {
    parameters = {
        {TextureParameter::WrapS, TextureValue::Linear},
        {TextureParameter::WrapT, TextureValue::Linear},
        {TextureParameter::MagFilter, TextureValue::Nearest},
        {TextureParameter::MinFilter, TextureValue::Nearest},
    };
  }
  return create_ref<Texture2DDescriptor>(id, path, flip_image_on_loading,
                                         parameters);
};

Ref<Texture3DDescriptor>
Texture3DDescriptor::create(const ResourceDescriptorID &id,
                            const std::vector<Ref<Path>> &face_paths) {
  return create_ref<Texture3DDescriptor>(id, face_paths);
};
} // namespace astralix
