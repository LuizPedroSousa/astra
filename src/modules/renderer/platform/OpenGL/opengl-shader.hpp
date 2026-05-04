#pragma once

#include "resources/shader.hpp"
#include "shader-lang/reflection.hpp"
#include <optional>
#include <unordered_map>

namespace astralix {

class OpenGLShader : public Shader {
public:
  OpenGLShader(const ResourceHandle &id, Ref<ShaderDescriptor> descriptor);

  ~OpenGLShader();

  void bind() const override;
  void unbind() const override;
  void attach() const override;
  bool recompile() override;
  std::vector<std::filesystem::path> source_dependencies() const override;
  uint32_t renderer_id() const override { return m_renderer_id; };

protected:
  void set_typed_value(uint64_t binding_id, ShaderValueKind kind,
                       const void *value) const override;

private:
  struct ProgramBinding {
    enum class Kind { UniformValue, SampledImage, UniformBlock, StorageBlock };

    Kind kind = Kind::UniformValue;
    uint64_t binding_id = 0;
    std::string logical_name;
    std::string emitted_name;
    int32_t location = -1;
    uint32_t block_index = static_cast<uint32_t>(-1);
    uint32_t descriptor_set = 0;
    uint32_t binding = 0;
  };

  void build_reflection_bindings(const ShaderReflection &reflection,
                                 std::unordered_map<std::string, ProgramBinding>
                                     &program_bindings,
                                 std::unordered_map<uint64_t, ProgramBinding>
                                     &program_bindings_by_id) const;
  void initialize_reflection_bindings(const ShaderReflection &reflection);
  uint32_t compile(Ref<Path> path, uint32_t type);
  uint32_t compile_glsl(const std::string &source, uint32_t type, const std::string &source_path = "");

  Ref<ShaderDescriptor> m_descriptor;
  uint32_t m_renderer_id = -1;
  uint32_t m_vertex_id = -1;
  uint32_t m_fragment_id = -1;
  uint32_t m_geometry_id = -1;
  uint32_t m_compute_id = -1;
  ShaderReflection m_reflection;
  std::vector<std::filesystem::path> m_source_dependencies;
  std::unordered_map<std::string, ProgramBinding> m_program_bindings;
  std::unordered_map<uint64_t, ProgramBinding> m_program_bindings_by_id;
};
} // namespace astralix
