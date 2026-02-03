#include "shader.hpp"
#include "glad/glad.h"
#include "guid.hpp"
#include "managers/resource-manager.hpp"
#include "platform/OpenGL/opengl-shader.hpp"
#include "renderer-api.hpp"

namespace astralix {

Ref<ShaderDescriptor> Shader::create(const ResourceDescriptorID &id,
                                     Ref<Path> fragment_path,
                                     Ref<Path> vertex_path,
                                     Ref<Path> geometry_path) {
  return resource_manager()->register_shader(
      ShaderDescriptor::create(id, fragment_path, vertex_path, geometry_path));
}

Ref<ShaderDescriptor> Shader::define(const ResourceDescriptorID &id,
                                     Ref<Path> fragment_path,
                                     Ref<Path> vertex_path,
                                     Ref<Path> geometry_path) {
  return ShaderDescriptor::create(id, fragment_path, vertex_path,
                                  geometry_path);
}

Ref<Shader> Shader::from_descriptor(const ResourceHandle &id,
                                    Ref<ShaderDescriptor> descriptor) {
  return create_renderer_component_ref<Shader, OpenGLShader>(
      descriptor->backend, id, descriptor);
}

} // namespace astralix
