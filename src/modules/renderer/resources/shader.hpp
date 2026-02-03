#pragma once

#include "base.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "guid.hpp"
#include "path.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/resource.hpp"

namespace astralix {

class Shader : public Resource {
public:
  virtual void bind() const = 0;
  virtual void unbind() const = 0;
  virtual void attach() const = 0;

  virtual void set_bool(const std::string &name, bool value) const = 0;
  virtual void set_int(const std::string &name, int value) const = 0;
  virtual void set_matrix(const std::string &name, glm::mat4 matrix) const = 0;
  virtual void set_float(const std::string &name, float value) const = 0;
  virtual void set_vec3(const std::string &name, glm::vec3 value) const = 0;

  virtual uint32_t renderer_id() const = 0;

  inline ResourceDescriptorID descriptor_id() const noexcept {
    return m_descriptor_id;
  }

  static Ref<ShaderDescriptor> create(const ResourceDescriptorID &id,
                                      Ref<Path> fragment_path,
                                      Ref<Path> vertex_path,
                                      Ref<Path> geometry_path = nullptr);

  static Ref<ShaderDescriptor> define(const ResourceDescriptorID &id,
                                      Ref<Path> fragment_path,
                                      Ref<Path> vertex_path,
                                      Ref<Path> geometry_path = nullptr);

  static Ref<Shader> from_descriptor(const ResourceHandle &id,
                                     Ref<ShaderDescriptor> descriptor);

protected:
  Shader(const ResourceHandle &id, const ResourceDescriptorID &descriptor_id)
      : Resource(id), m_descriptor_id(descriptor_id) {}

  ResourceDescriptorID m_descriptor_id;
};

} // namespace astralix
