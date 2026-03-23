#pragma once

#include "resources/shader.hpp"
#include "shader-lang/reflection.hpp"
#include <unordered_map>

namespace astralix {

class OpenGLShader : public Shader {
public:
  OpenGLShader(const ResourceHandle &id, Ref<ShaderDescriptor> descriptor);

  ~OpenGLShader();

  void bind() const override;
  void unbind() const override;
  void attach() const override;

  void set_bool(const std::string &name, bool value) const override;
  void set_int(const std::string &name, int value) const override;
  void set_matrix(const std::string &name, glm::mat4 matrix) const override;
  void set_float(const std::string &name, float value) const override;
  void set_vec3(const std::string &name, glm::vec3 value) const override;
  uint32_t renderer_id() const override { return m_renderer_id; };

protected:
  void set_typed_value(uint64_t binding_id, ShaderValueKind kind,
                       const void *value) const override;

private:
  struct ProgramBinding {
    enum class Kind { Uniform, UniformBlock, StorageBlock };

    Kind kind = Kind::Uniform;
    std::string logical_name;
    std::string emitted_name;
    int32_t location = -1;
    uint32_t block_index = static_cast<uint32_t>(-1);
    uint32_t binding = 0;
  };

  void build_reflection_bindings(const ShaderReflection &reflection,
                                 std::unordered_map<std::string, ProgramBinding>
                                     &program_bindings,
                                 std::unordered_map<uint64_t, ProgramBinding>
                                     &program_bindings_by_id) const;
  void initialize_reflection_bindings(const ShaderReflection &reflection);
  int32_t resolve_uniform_location(const std::string &name) const;
  uint32_t compile(Ref<Path> path, uint32_t type);
  uint32_t compile_glsl(const std::string &source, uint32_t type);

  uint32_t m_renderer_id = -1;
  uint32_t m_vertex_id = -1;
  uint32_t m_fragment_id = -1;
  uint32_t m_geometry_id = -1;
  ShaderReflection m_reflection;
  mutable std::unordered_map<std::string, int32_t> m_uniform_locations;
  std::unordered_map<std::string, ProgramBinding> m_program_bindings;
  std::unordered_map<uint64_t, ProgramBinding> m_program_bindings_by_id;
};
} // namespace astralix
