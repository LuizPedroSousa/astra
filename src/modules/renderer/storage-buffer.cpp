#include "storage-buffer.hpp"

#include "platform/OpenGL/opengl-storage-buffer.hpp"
#include "renderer-api.hpp"
#include "virtual-storage-buffer.hpp"

namespace astralix {

Ref<StorageBuffer> StorageBuffer::create(RendererBackend backend,
                                         uint32_t size) {
  switch (backend) {
  case RendererBackend::OpenGL:
    return create_ref<OpenGLStorageBuffer>(size);
  case RendererBackend::Vulkan:
    return create_ref<VirtualStorageBuffer>(size);
  default:
    ASTRA_EXCEPTION("NONE ins't a valid renderer api");
  }
}

} // namespace astralix
