#include "texture.hpp"
#include "assert.hpp"
#include "filesystem"
#include "glad/glad.h"
#include "guid.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "platform/OpenGL/opengl-texture2D.hpp"
#include "platform/OpenGL/opengl-texture3D.hpp"
#include "renderer-api.hpp"
#include "stb_image/stb_image.h"

namespace astralix {

void Texture::free_image(u_char *data) { stbi_image_free(data); }

int Texture::get_image_format(int nr_channels) {
  switch (nr_channels) {
  case 1:
    return GL_RED;
  case 3:
    return GL_RGB;

  default:
    return GL_RGBA;
  }
}

Image Texture::load_image(Ref<Path> path, bool flip_image_on_loading) {
  int width, height, nr_channels;

  unsigned char *data;

  auto resolved_path = PathManager::get()->resolve(path);

  stbi_set_flip_vertically_on_load(flip_image_on_loading);

  data = stbi_load(resolved_path.c_str(), &width, &height, &nr_channels, 0);

  if (!data) {
    free_image(data);
    ASTRA_EXCEPTION("Cant't load image: ", path);
  }

  return Image{width, height, nr_channels, data};
};

Ref<Texture2DDescriptor> Texture2D::create(
    const ResourceDescriptorID &id, Ref<Path> path,
    const bool flip_image_on_loading,
    std::unordered_map<TextureParameter, TextureValue> parameters) {

  return resource_manager()->register_texture(
      Texture2DDescriptor::create(id, path, flip_image_on_loading, parameters));
};

Ref<Texture2DDescriptor>
Texture2D::create(const ResourceDescriptorID &resource_id,
                  TextureConfig config) {
  return resource_manager()->register_texture(
      Texture2DDescriptor::create(resource_id, config));
};

Ref<Texture2D> Texture2D::from_descriptor(const ResourceHandle &id,
                                          Ref<Texture2DDescriptor> descriptor) {
  return create_renderer_component_ref<Texture2D, OpenGLTexture2D>(
      descriptor->backend, id, descriptor);
};

Ref<Texture3DDescriptor>
Texture3D::create(const ResourceDescriptorID &id,
                  const std::vector<Ref<Path>> &faces_path) {
  return resource_manager()->register_texture(
      Texture3DDescriptor::create(id, faces_path));
};

Ref<Texture3DDescriptor>
Texture3D::define(const ResourceDescriptorID &id,
                  const std::vector<Ref<Path>> &faces_path) {
  return Texture3DDescriptor::create(id, faces_path);
};

Ref<Texture3D> Texture3D::from_descriptor(const ResourceHandle &id,
                                          Ref<Texture3DDescriptor> descriptor) {
  return create_renderer_component_ref<Texture3D, OpenGLTexture3D>(
      descriptor->backend, id, descriptor);
};

} // namespace astralix
