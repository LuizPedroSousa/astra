#include "shaderc-compiler.hpp"

#include <shaderc/shaderc.hpp>

namespace astralix {

namespace {

shaderc_shader_kind to_shaderc_kind(StageKind stage) {
  switch (stage) {
  case StageKind::Vertex:
    return shaderc_vertex_shader;
  case StageKind::Fragment:
    return shaderc_fragment_shader;
  case StageKind::Geometry:
    return shaderc_geometry_shader;
  default:
    return shaderc_glsl_infer_from_source;
  }
}

} // namespace

ShadercCompileResult compile_glsl_to_spirv(std::string_view glsl_source,
                                           StageKind stage,
                                           std::string_view filename) {
  ShadercCompileResult result;

  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetTargetEnvironment(shaderc_target_env_vulkan,
                               shaderc_env_version_vulkan_1_0);
  options.SetSourceLanguage(shaderc_source_language_glsl);

  auto module = compiler.CompileGlslToSpv(
      glsl_source.data(), glsl_source.size(), to_shaderc_kind(stage),
      std::string(filename).c_str(), options);

  if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
    result.errors.push_back(module.GetErrorMessage());
    return result;
  }

  result.spirv.assign(module.cbegin(), module.cend());
  return result;
}

} // namespace astralix
