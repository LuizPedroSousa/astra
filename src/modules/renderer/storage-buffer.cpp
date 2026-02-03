#include "storage-buffer.hpp"

#include "engine.hpp"
#include "platform/OpenGL/opengl-storage-buffer.hpp"
#include "renderer-api.hpp"

namespace astralix {

Ref<StorageBuffer> StorageBuffer::create(RendererBackend backend,
                                         uint32_t size) {
  return create_renderer_component_ref<StorageBuffer, OpenGLStorageBuffer>(
      backend, size);
}

} // namespace astralix
