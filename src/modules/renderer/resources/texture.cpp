#include "texture.hpp"
#include "assert.hpp"
#include <cstring>
#include "filesystem"
#include "glad/glad.h"
#include "guid.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "platform/OpenGL/opengl-texture2D.hpp"
#include "platform/OpenGL/opengl-texture3D.hpp"
#include "renderer-api.hpp"
#include "stb_image/stb_image.h"
#include "tinyexr/tinyexr.h"
#include "virtual-texture2D.hpp"
#include "virtual-texture3D.hpp"

namespace astralix {

namespace {

PreparedTexture2DData copy_loaded_texture_data(const Image &image) {
  PreparedTexture2DData prepared;
  prepared.width = static_cast<uint32_t>(std::max(image.width, 1));
  prepared.height = static_cast<uint32_t>(std::max(image.height, 1));
  prepared.nr_channels = image.nr_channels;

  const size_t byte_count =
      static_cast<size_t>(prepared.width) *
      static_cast<size_t>(prepared.height) *
      static_cast<size_t>(std::max(image.nr_channels, 1));
  prepared.bytes.resize(byte_count, 0);
  if (image.data != nullptr && byte_count > 0u) {
    std::memcpy(prepared.bytes.data(), image.data, byte_count);
  }

  return prepared;
}

} // namespace

void Texture::free_image(u_char *data) { stbi_image_free(data); }

void Texture::free_hdr_image(float *data) {
  if (data != nullptr) {
    free(data);
  }
}

HDRImage Texture::load_hdr_image(Ref<Path> path, bool flip_image_on_loading) {
  auto resolved_path = PathManager::get()->resolve(path);

  auto extension = resolved_path.extension().string();
  if (extension == ".exr" || extension == ".EXR") {
    float *pixel_data = nullptr;
    int width = 0;
    int height = 0;
    const char *error_message = nullptr;

    int load_result = LoadEXR(
        &pixel_data, &width, &height, resolved_path.c_str(), &error_message
    );

    if (load_result != TINYEXR_SUCCESS) {
      std::string message = error_message ? error_message : "unknown error";
      if (error_message) {
        FreeEXRErrorMessage(error_message);
      }
      ASTRA_EXCEPTION("Failed to load EXR: ", resolved_path, " (", message, ")");
    }

    if (flip_image_on_loading) {
      const int row_stride = width * 4;
      std::vector<float> temp_row(row_stride);
      for (int row = 0; row < height / 2; ++row) {
        float *top = pixel_data + row * row_stride;
        float *bottom = pixel_data + (height - 1 - row) * row_stride;
        std::memcpy(temp_row.data(), top, row_stride * sizeof(float));
        std::memcpy(top, bottom, row_stride * sizeof(float));
        std::memcpy(bottom, temp_row.data(), row_stride * sizeof(float));
      }
    }

    return HDRImage{width, height, 4, pixel_data};
  }

  stbi_set_flip_vertically_on_load(flip_image_on_loading);

  int width = 0;
  int height = 0;
  int nr_channels = 0;
  float *data = stbi_loadf(resolved_path.c_str(), &width, &height, &nr_channels, 4);

  if (data == nullptr) {
    const char *reason = stbi_failure_reason();
    ASTRA_EXCEPTION(
        "Failed to load HDR image: ", resolved_path,
        reason != nullptr ? " (" : "",
        reason != nullptr ? reason : "",
        reason != nullptr ? ")" : ""
    );
  }

  return HDRImage{width, height, 4, data};
}

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
    const char *reason = stbi_failure_reason();
    ASTRA_EXCEPTION(
        "Can't load image: ",
        resolved_path,
        reason != nullptr ? " (" : "",
        reason != nullptr ? reason : "",
        reason != nullptr ? ")" : ""
    );
  }

  return Image{width, height, nr_channels, data};
};

Ref<Texture2DDescriptor> Texture2D::create(
    const ResourceDescriptorID &id, Ref<Path> path,
    const bool flip_image_on_loading,
    std::unordered_map<TextureParameter, TextureValue> parameters
) {

  return resource_manager()->register_texture(
      Texture2DDescriptor::create(id, path, flip_image_on_loading, parameters)
  );
};

Ref<Texture2DDescriptor>
Texture2D::create(const ResourceDescriptorID &resource_id, TextureConfig config) {
  return resource_manager()->register_texture(
      Texture2DDescriptor::create(resource_id, config)
  );
};

Ref<Texture2DDescriptor>
Texture2D::define(const ResourceDescriptorID &resource_id, TextureConfig config) {
  return Texture2DDescriptor::create(resource_id, config);
};

PreparedTexture2DData
Texture2D::prepare_descriptor(Ref<Texture2DDescriptor> descriptor) {
  ASTRA_ENSURE(descriptor == nullptr, "Missing texture descriptor");
  ASTRA_ENSURE(
      !descriptor->image_load.has_value(),
      "Texture descriptor does not support background preparation: ",
      descriptor->id
  );

  auto image = load_image(
      descriptor->image_load->path,
      descriptor->image_load->flip_image_on_loading
  );
  auto prepared = copy_loaded_texture_data(image);
  if (image.data != nullptr) {
    free_image(image.data);
  }
  return prepared;
}

Ref<Texture2D> Texture2D::from_descriptor(const ResourceHandle &id, Ref<Texture2DDescriptor> descriptor) {
  switch (descriptor->backend) {
  case RendererBackend::OpenGL:
    return create_ref<OpenGLTexture2D>(id, descriptor);
  case RendererBackend::Vulkan:
    return create_ref<VirtualTexture2D>(id, descriptor);
  default:
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
};

Ref<Texture2D> Texture2D::from_prepared_descriptor(
    const ResourceHandle &id,
    Ref<Texture2DDescriptor> descriptor,
    PreparedTexture2DData prepared
) {
  switch (descriptor->backend) {
  case RendererBackend::OpenGL:
    return create_ref<OpenGLTexture2D>(id, descriptor, std::move(prepared));
  case RendererBackend::Vulkan:
    return create_ref<VirtualTexture2D>(id, descriptor, std::move(prepared));
  default:
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
}

Ref<Texture3DDescriptor>
Texture3D::create(const ResourceDescriptorID &id, const std::vector<Ref<Path>> &faces_path) {
  return resource_manager()->register_texture(
      Texture3DDescriptor::create(id, faces_path)
  );
};

Ref<Texture3DDescriptor>
Texture3D::create_from_equirectangular(
    const ResourceDescriptorID &id,
    Ref<Path> equirectangular_path,
    uint32_t face_resolution
) {
  return resource_manager()->register_texture(
      Texture3DDescriptor::create_from_equirectangular(id, equirectangular_path, face_resolution)
  );
};

Ref<Texture3DDescriptor>
Texture3D::create_from_buffer(
    const ResourceDescriptorID &id,
    uint32_t face_width,
    uint32_t face_height,
    const std::vector<const unsigned char *> &face_buffers
) {
  return resource_manager()->register_texture(
      Texture3DDescriptor::create_from_buffer(id, face_width, face_height, face_buffers)
  );
};

Ref<Texture3DDescriptor>
Texture3D::create_from_float_buffer(
    const ResourceDescriptorID &id,
    uint32_t face_width,
    uint32_t face_height,
    std::vector<std::vector<float>> float_face_data
) {
  return resource_manager()->register_texture(
      Texture3DDescriptor::create_from_float_buffer(
          id, face_width, face_height, std::move(float_face_data)
      )
  );
};

Ref<Texture3DDescriptor>
Texture3D::define(const ResourceDescriptorID &id, const std::vector<Ref<Path>> &faces_path) {
  return Texture3DDescriptor::create(id, faces_path);
};

Ref<Texture3D> Texture3D::from_descriptor(const ResourceHandle &id, Ref<Texture3DDescriptor> descriptor) {
  switch (descriptor->backend) {
  case RendererBackend::OpenGL:
    return create_ref<OpenGLTexture3D>(id, descriptor);
  case RendererBackend::Vulkan:
    return create_ref<VirtualTexture3D>(id, descriptor);
  default:
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
};

} // namespace astralix
