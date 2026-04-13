#pragma once

#include "resources/shader.hpp"
#include "shader-lang/reflection.hpp"

namespace astralix {

class VirtualShader : public Shader {
public:
  VirtualShader(const ResourceHandle &id, Ref<ShaderDescriptor> descriptor);
  ~VirtualShader() = default;

  void bind() const override;
  void unbind() const override;
  void attach() const override;

  uint32_t renderer_id() const override { return 0; }

protected:
  void set_typed_value(uint64_t binding_id, ShaderValueKind kind,
                       const void *value) const override;

private:
  void load_reflection_from_path(const Ref<Path> &path);
  void merge_reflection(const ShaderReflection &reflection);

  ShaderReflection m_reflection;
};

} // namespace astralix
