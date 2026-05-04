#include "virtual-shader.hpp"

#include "assert.hpp"
#include "managers/path-manager.hpp"
#include "shader-lang/compiler.hpp"
#include "shader-lang/reflection-serializer.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace astralix {

namespace {

void validate_shader_extension(const std::filesystem::path &resolved_path) {
  const auto extension = resolved_path.extension().string();
  ASTRA_ENSURE(extension != ".axsl" && extension != ".glsl",
               "Unsupported shader file extension '", extension, "' in '",
               resolved_path.string(), "': expected .axsl or .glsl");
}

std::optional<ShaderReflection>
load_glsl_reflection(const std::filesystem::path &resolved_path) {
  const std::string filename = resolved_path.filename().string();
  static constexpr std::pair<StageKind, const char *> stage_extensions[] = {
      {StageKind::Vertex, "vert"},
      {StageKind::Fragment, "frag"},
      {StageKind::Geometry, "geom"},
      {StageKind::Compute, "comp"},
  };

  for (auto [stage, ext] : stage_extensions) {
    (void)stage;
    const std::string suffix = std::string(".") + ext + ".glsl";
    if (!filename.ends_with(suffix)) {
      continue;
    }

    const auto reflection_path =
        resolved_path.parent_path() /
        (filename.substr(0, filename.size() - suffix.size()) +
         ".reflection.json");
    if (!std::filesystem::exists(reflection_path)) {
      return std::nullopt;
    }

    std::string error;
    return read_shader_reflection(reflection_path, SerializationFormat::Json,
                                  &error);
  }

  return std::nullopt;
}

ShaderReflection compile_axsl_reflection(
    const std::filesystem::path &resolved_path) {
  std::ifstream file(resolved_path);
  ASTRA_ENSURE(!file, "Failed to open shader source: ", resolved_path.string());

  std::ostringstream buffer;
  buffer << file.rdbuf();

  Compiler compiler;
  const auto result =
      compiler.compile(buffer.str(), resolved_path.parent_path().string(),
                       resolved_path.string(),
                       {.emit_reflection_ir = true});
  if (result.ok()) {
    return result.reflection;
  }

  std::string error_message;
  for (const auto &error : result.errors) {
    if (!error_message.empty()) {
      error_message += "\n";
    }
    error_message += error;
  }

  ASTRA_EXCEPTION("Failed to compile shader reflection for '",
                  resolved_path.string(), "':\n", error_message);
}

} // namespace

VirtualShader::VirtualShader(const ResourceHandle &id,
                             Ref<ShaderDescriptor> descriptor)
    : Shader(id, descriptor->id) {
  load_reflection_from_path(descriptor->vertex_path);
  load_reflection_from_path(descriptor->fragment_path);
  load_reflection_from_path(descriptor->geometry_path);
  load_reflection_from_path(descriptor->compute_path);
}

void VirtualShader::bind() const {}

void VirtualShader::unbind() const {}

void VirtualShader::attach() const {}

void VirtualShader::set_typed_value(uint64_t binding_id, ShaderValueKind kind,
                                    const void *value) const {
  (void)binding_id;
  (void)kind;
  (void)value;
}

void VirtualShader::load_reflection_from_path(const Ref<Path> &path) {
  if (path == nullptr) {
    return;
  }

  const auto resolved_path = PathManager::get()->resolve(path);
  validate_shader_extension(resolved_path);

  if (resolved_path.extension() == ".axsl") {
    merge_reflection(compile_axsl_reflection(resolved_path));
    return;
  }

  if (auto reflection = load_glsl_reflection(resolved_path); reflection) {
    merge_reflection(*reflection);
  }
}

void VirtualShader::merge_reflection(const ShaderReflection &reflection) {
  for (const auto &[stage, stage_reflection] : reflection.stages) {
    m_reflection.stages[stage] = stage_reflection;
  }
}

} // namespace astralix
