#include "opengl-shader.hpp"
#include "assert.hpp"
#include "glad/glad.h"
#include "log.hpp"

#include "filesystem"
#include "fstream"
#include "iostream"
#include "managers/path-manager.hpp"
#include "resources/shader.hpp"
#include "shader-lang/compiler.hpp"
#include "shader-lang/reflection-serializer.hpp"
#include "sstream"
#include <cstring>
#include <unordered_set>

namespace astralix {
OpenGLShader::OpenGLShader(const ResourceHandle &resource_id,
                           Ref<ShaderDescriptor> descriptor)
    : Shader(resource_id, descriptor->id) {
  static constexpr int k_axsl_glsl_cache_version = 2;

  Compiler compiler;

  auto vertex_path = PathManager::get()->resolve(descriptor->vertex_path);
  auto fragment_path = PathManager::get()->resolve(descriptor->fragment_path);

  auto validate_extension = [](const std::filesystem::path &resolved) {
    auto extension = resolved.extension().string();

    ASTRA_ENSURE(extension != ".axsl" && extension != ".glsl",
                 "Unsupported shader file extension '" + extension + "' in '" +
                     resolved.string() + "': expected .axsl or .glsl");
  };

  validate_extension(vertex_path);
  validate_extension(fragment_path);

  std::filesystem::path geometry_path;

  if (descriptor->geometry_path) {
    geometry_path = PathManager::get()->resolve(descriptor->geometry_path);
    validate_extension(geometry_path);
  }

  static constexpr std::pair<StageKind, const char *> stage_extensions[] = {
      {StageKind::Vertex, "vert"},
      {StageKind::Fragment, "frag"},
      {StageKind::Geometry, "geom"},
      {StageKind::Compute, "comp"},
  };

  auto cache_path = [&](const std::filesystem::path &resolved,
                        const char *ext) {
    return resolved.parent_path() /
           (resolved.stem().string() + "." + ext + ".glsl");
  };
  auto cache_version_path = [&](const std::filesystem::path &resolved) {
    return resolved.parent_path() /
           (resolved.stem().string() + ".glsl.cache.version");
  };

  std::map<std::string, CompileResult> axsl_results;

  auto ensure_compiled = [&](const std::filesystem::path &resolved) {
    if (resolved.extension() != ".axsl") {
      return;
    }

    auto key = resolved.string();
    auto reflection_path = shader_reflection_sidecar_path(resolved);
    auto version_path = cache_version_path(resolved);

    if (axsl_results.count(key)) {
      return;
    }

    auto source_mtime = std::filesystem::last_write_time(resolved);
    bool any_exists = false;
    bool any_stale = false;
    bool reflection_ready = std::filesystem::exists(reflection_path);
    bool cache_version_ready = false;

    if (reflection_ready &&
        std::filesystem::last_write_time(reflection_path) < source_mtime) {
      reflection_ready = false;
    }

    if (std::filesystem::exists(version_path)) {
      std::ifstream version_file(version_path);
      int cached_version = 0;
      if (version_file >> cached_version) {
        cache_version_ready = cached_version == k_axsl_glsl_cache_version;
      }
    }

    for (auto [kind, ext] : stage_extensions) {
      auto cached = cache_path(resolved, ext);

      if (!std::filesystem::exists(cached)) {
        continue;
      }

      any_exists = true;
      if (std::filesystem::last_write_time(cached) < source_mtime) {
        any_stale = true;
        break;
      }
    }

    if (any_exists && !any_stale && reflection_ready && cache_version_ready) {
      CompileResult cached_result;
      for (auto [kind, ext] : stage_extensions) {
        auto cached = cache_path(resolved, ext);
        if (!std::filesystem::exists(cached))
          continue;
        std::ifstream file(cached);
        std::ostringstream buffer;
        buffer << file.rdbuf();
        cached_result.stages[kind] = buffer.str();
      }

      std::string reflection_error;
      auto cached_reflection =
          read_shader_reflection(reflection_path, SerializationFormat::Json,
                                 &reflection_error);

      if (cached_reflection.has_value() &&
          cached_reflection->version == k_shader_reflection_version) {
        cached_result.reflection = std::move(*cached_reflection);
        axsl_results[key] = std::move(cached_result);
        return;
      }
    }

    std::ifstream file(resolved);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    auto result = compiler.compile(
        buffer.str(), resolved.parent_path().string(), resolved.string(),
        {
            .emit_reflection_ir = true,
            .reflection_ir_format = SerializationFormat::Json,
            .emit_pipeline_layout_ir = true,
            .pipeline_layout_ir_format = SerializationFormat::Json,
        });

    if (result.ok()) {
      for (auto [kind, ext] : stage_extensions) {
        auto it = result.stages.find(kind);

        if (it == result.stages.end()) {
          continue;
        }
        std::ofstream out(cache_path(resolved, ext));
        out << it->second;
      }

      ASTRA_ENSURE(!result.reflection_ir.has_value(),
                   "AXSL compile result is missing emitted reflection IR");

      std::ofstream reflection_out(reflection_path,
                                   std::ios::binary | std::ios::trunc);
      ASTRA_ENSURE(!reflection_out,
                   "cannot write reflection sidecar '" +
                       reflection_path.string() + "'");
      reflection_out << result.reflection_ir->content;
      ASTRA_ENSURE(!reflection_out.good(),
                   "cannot write reflection sidecar '" +
                       reflection_path.string() + "'");

      std::ofstream version_out(version_path, std::ios::trunc);
      ASTRA_ENSURE(!version_out,
                   "cannot write shader cache version sidecar '" +
                       version_path.string() + "'");
      version_out << k_axsl_glsl_cache_version;
      ASTRA_ENSURE(!version_out.good(),
                   "cannot write shader cache version sidecar '" +
                       version_path.string() + "'");
    }

    axsl_results[key] = std::move(result);
  };

  ensure_compiled(vertex_path);
  ensure_compiled(fragment_path);

  if (!geometry_path.empty()) {
    ensure_compiled(geometry_path);
  }

  for (auto &[path, result] : axsl_results) {
    if (!result.ok()) {
      std::string error_message;

      for (auto &error : result.errors) {
        error_message += error + "\n";
      }
      ASTRA_EXCEPTION(error_message);
    }
  }

  auto load_stage = [&](const std::filesystem::path &resolved, StageKind kind,
                        uint32_t gl_type) -> uint32_t {
    if (resolved.extension() == ".axsl") {
      auto &stages = axsl_results.at(resolved.string()).stages;
      auto it = stages.find(kind);
      return it != stages.end() ? compile_glsl(it->second, gl_type)
                                : static_cast<uint32_t>(-1);
    }
    std::ifstream file(resolved);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return compile_glsl(buffer.str(), gl_type);
  };

  m_renderer_id = glCreateProgram();
  m_vertex_id = load_stage(vertex_path, StageKind::Vertex, GL_VERTEX_SHADER);
  m_fragment_id =
      load_stage(fragment_path, StageKind::Fragment, GL_FRAGMENT_SHADER);

  if (!geometry_path.empty())
    m_geometry_id =
        load_stage(geometry_path, StageKind::Geometry, GL_GEOMETRY_SHADER);

  attach();

  auto merge_reflection = [&](const ShaderReflection &reflection) {
    for (const auto &[stage, stage_reflection] : reflection.stages) {
      m_reflection.stages[stage] = stage_reflection;
    }
  };

  auto try_load_stage_reflection = [&](const std::filesystem::path &resolved) {
    if (resolved.empty()) {
      return;
    }

    if (resolved.extension() == ".axsl") {
      merge_reflection(axsl_results.at(resolved.string()).reflection);
      return;
    }

    if (resolved.extension() != ".glsl") {
      return;
    }

    const std::string filename = resolved.filename().string();
    for (auto [kind, ext] : stage_extensions) {
      (void)kind;
      const std::string suffix = std::string(".") + ext + ".glsl";
      if (!filename.ends_with(suffix)) {
        continue;
      }

      const auto base = resolved.parent_path() /
                        (filename.substr(0, filename.size() - suffix.size()) +
                         ".reflection.json");
      if (!std::filesystem::exists(base)) {
        return;
      }

      std::string reflection_error;
      auto reflection =
          read_shader_reflection(base, SerializationFormat::Json,
                                 &reflection_error);
      if (reflection &&
          reflection->version == k_shader_reflection_version) {
        merge_reflection(*reflection);
      }
      return;
    }
  };

  try_load_stage_reflection(vertex_path);
  try_load_stage_reflection(fragment_path);
  if (!geometry_path.empty()) {
    try_load_stage_reflection(geometry_path);
  }

  initialize_reflection_bindings(m_reflection);
}

OpenGLShader::~OpenGLShader() { glDeleteProgram(m_renderer_id); }

void OpenGLShader::bind() const { glUseProgram(m_renderer_id); }
void OpenGLShader::unbind() const { glUseProgram(0); }

void OpenGLShader::attach() const {
  ASTRA_ENSURE(m_renderer_id == 0, "Shader not found");

  glAttachShader(m_renderer_id, m_vertex_id);
  glAttachShader(m_renderer_id, m_fragment_id);

  if (m_geometry_id != -1) {
    glAttachShader(m_renderer_id, m_geometry_id);
  }

  int success;

  glLinkProgram(m_renderer_id);

  glGetProgramiv(m_renderer_id, GL_LINK_STATUS, &success);

  if (!success) {
    char *infoLog = new char[512];

    glGetProgramInfoLog(m_renderer_id, 512, NULL, infoLog);

    ASTRA_EXCEPTION(infoLog);
  };

  glDeleteShader(m_vertex_id);
  glDeleteShader(m_fragment_id);
  if (m_geometry_id != -1)
    glDeleteShader(m_geometry_id);
}

void OpenGLShader::build_reflection_bindings(
    const ShaderReflection &reflection,
    std::unordered_map<std::string, ProgramBinding> &program_bindings,
    std::unordered_map<uint64_t, ProgramBinding> &program_bindings_by_id) const {
  program_bindings.clear();
  program_bindings_by_id.clear();

  std::unordered_map<std::string, std::string> alias_targets;
  std::unordered_set<std::string> ambiguous_aliases;

  auto register_alias = [&](const std::optional<std::string> &alias,
                            const std::string &logical_name) {
    if (!alias) {
      return;
    }

    auto inserted = alias_targets.emplace(*alias, logical_name);
    if (!inserted.second && inserted.first->second != logical_name) {
      ambiguous_aliases.insert(*alias);
    }
  };

  auto register_binding = [&](ProgramBinding binding,
                              std::optional<uint64_t> binding_id =
                                  std::nullopt) {
    auto inserted =
        program_bindings.emplace(binding.logical_name, binding);
    if (!inserted.second) {
      if (inserted.first->second.emitted_name.empty()) {
        inserted.first->second.emitted_name = binding.emitted_name;
      }
      if (binding.binding != 0) {
        inserted.first->second.binding = binding.binding;
      }
    }

    if (!binding_id) {
      return;
    }

    auto id_inserted =
        program_bindings_by_id.emplace(*binding_id, binding);
    if (!id_inserted.second) {
      if (id_inserted.first->second.emitted_name.empty()) {
        id_inserted.first->second.emitted_name = binding.emitted_name;
      }
      if (binding.binding != 0) {
        id_inserted.first->second.binding = binding.binding;
      }
    }
  };

  for (const auto &[stage_kind, stage] : reflection.stages) {
    (void)stage_kind;

    for (const auto &resource : stage.resources) {
      if (resource.kind == ShaderResourceKind::UniformInterface) {
        for (const auto &member : resource.members) {
          if (!member.glsl.emitted_name) {
            continue;
          }

          register_binding(
              ProgramBinding{
                  .kind = ProgramBinding::Kind::UniformValue,
                  .binding_id = member.binding_id,
                  .logical_name = member.logical_name,
                  .emitted_name = *member.glsl.emitted_name,
              },
              member.binding_id);
          register_alias(member.compatibility_alias, member.logical_name);
        }
        continue;
      }

      if (resource.kind == ShaderResourceKind::UniformBlock ||
          resource.kind == ShaderResourceKind::StorageBuffer) {
        const auto &name = resource.glsl.block_name.value_or(
            resource.glsl.emitted_name.value_or(""));
        if (name.empty()) {
          continue;
        }

        auto binding_kind = resource.kind == ShaderResourceKind::UniformBlock
                                ? ProgramBinding::Kind::UniformBlock
                                : ProgramBinding::Kind::StorageBlock;

        register_binding(ProgramBinding{
            .kind = binding_kind,
            .binding_id = resource.binding_id,
            .logical_name = resource.logical_name,
            .emitted_name = name,
            .binding = resource.glsl.binding.value_or(0),
        });
        continue;
      }

      if (resource.kind == ShaderResourceKind::Sampler) {
        if (!resource.glsl.emitted_name) {
          continue;
        }

        register_binding(
            ProgramBinding{
                .kind = ProgramBinding::Kind::SampledImage,
                .binding_id = resource.binding_id,
                .logical_name = resource.logical_name,
                .emitted_name = *resource.glsl.emitted_name,
                .descriptor_set = resource.glsl.descriptor_set.value_or(0),
                .binding = resource.glsl.binding.value_or(0),
            },
            resource.binding_id);
        continue;
      }

      if (resource.members.empty()) {
        if (!resource.glsl.emitted_name) {
          continue;
        }

        register_binding(ProgramBinding{
            .kind = ProgramBinding::Kind::UniformValue,
            .binding_id = resource.binding_id,
            .logical_name = resource.logical_name,
            .emitted_name = *resource.glsl.emitted_name,
        });
        continue;
      }

      for (const auto &member : resource.members) {
        if (!member.glsl.emitted_name) {
          continue;
        }

        register_binding(
            ProgramBinding{
                .kind = ProgramBinding::Kind::UniformValue,
                .binding_id = member.binding_id,
                .logical_name = member.logical_name,
                .emitted_name = *member.glsl.emitted_name,
            },
            member.binding_id);
        register_alias(member.compatibility_alias, member.logical_name);
      }
    }
  }

  for (const auto &[alias, logical_name] : alias_targets) {
    if (ambiguous_aliases.count(alias) > 0) {
      continue;
    }

    auto binding_it = program_bindings.find(logical_name);
    if (binding_it == program_bindings.end()) {
      continue;
    }

    ProgramBinding aliased = binding_it->second;
    aliased.logical_name = alias;
    program_bindings.emplace(alias, aliased);
  }
}

void OpenGLShader::initialize_reflection_bindings(
    const ShaderReflection &reflection) {
  build_reflection_bindings(reflection, m_program_bindings,
                            m_program_bindings_by_id);

  if (reflection.empty()) {
    return;
  }

  for (auto &[logical_name, binding] : m_program_bindings) {
    switch (binding.kind) {
      case ProgramBinding::Kind::UniformValue:
        binding.location =
            glGetUniformLocation(m_renderer_id, binding.emitted_name.c_str());
        break;
      case ProgramBinding::Kind::SampledImage:
        binding.location =
            glGetUniformLocation(m_renderer_id, binding.emitted_name.c_str());
        break;
      case ProgramBinding::Kind::UniformBlock:
        binding.block_index =
            glGetUniformBlockIndex(m_renderer_id, binding.emitted_name.c_str());
        if (binding.block_index != static_cast<uint32_t>(GL_INVALID_INDEX)) {
          glUniformBlockBinding(m_renderer_id, binding.block_index,
                                binding.binding);
        }
        break;
      case ProgramBinding::Kind::StorageBlock:
        binding.block_index =
            glGetProgramResourceIndex(m_renderer_id, GL_SHADER_STORAGE_BLOCK,
                                      binding.emitted_name.c_str());
        if (binding.block_index != static_cast<uint32_t>(GL_INVALID_INDEX)) {
          glShaderStorageBlockBinding(m_renderer_id, binding.block_index,
                                      binding.binding);
        }
        break;
      }
  }

  for (auto &[binding_id, binding] : m_program_bindings_by_id) {
    if (binding.kind != ProgramBinding::Kind::UniformValue &&
        binding.kind != ProgramBinding::Kind::SampledImage) {
      continue;
    }

    auto binding_it = m_program_bindings.find(binding.logical_name);
    if (binding_it == m_program_bindings.end()) {
      continue;
    }

    binding.location = binding_it->second.location;
  }
}

void OpenGLShader::set_typed_value(uint64_t binding_id, ShaderValueKind kind,
                                   const void *value) const {
  auto binding_it = m_program_bindings_by_id.find(binding_id);
  if (binding_it == m_program_bindings_by_id.end()) {
    return;
  }

  const auto &binding = binding_it->second;
  if (binding.kind != ProgramBinding::Kind::UniformValue &&
      binding.kind != ProgramBinding::Kind::SampledImage) {
    return;
  }
  if (binding.location < 0) {
    return;
  }

  switch (kind) {
    case ShaderValueKind::Bool:
      glUniform1i(binding.location, static_cast<int>(*static_cast<const bool *>(value)));
      return;
    case ShaderValueKind::Int:
      glUniform1i(binding.location, *static_cast<const int *>(value));
      return;
    case ShaderValueKind::Float:
      glUniform1f(binding.location, *static_cast<const float *>(value));
      return;
    case ShaderValueKind::Vec2: {
      const auto &typed_value = *static_cast<const glm::vec2 *>(value);
      glUniform2f(binding.location, typed_value.x, typed_value.y);
      return;
    }
    case ShaderValueKind::Vec3: {
      const auto &typed_value = *static_cast<const glm::vec3 *>(value);
      glUniform3f(binding.location, typed_value.x, typed_value.y, typed_value.z);
      return;
    }
    case ShaderValueKind::Vec4: {
      const auto &typed_value = *static_cast<const glm::vec4 *>(value);
      glUniform4f(binding.location, typed_value.x, typed_value.y, typed_value.z,
                  typed_value.w);
      return;
    }
    case ShaderValueKind::Mat3:
      glUniformMatrix3fv(binding.location, 1, GL_FALSE,
                         glm::value_ptr(*static_cast<const glm::mat3 *>(value)));
      return;
    case ShaderValueKind::Mat4:
      glUniformMatrix4fv(binding.location, 1, GL_FALSE,
                         glm::value_ptr(*static_cast<const glm::mat4 *>(value)));
      return;
  }
}

static std::string get_file_content_str(Ref<Path> filename) {
  auto path = PathManager::get()->resolve(filename);

  LOG_INFO(path);

  std::ifstream file(path);
  std::stringstream buffer;

  buffer << file.rdbuf();

  return buffer.str();
}

uint32_t OpenGLShader::compile_glsl(const std::string &source, uint32_t type) {
  uint32_t shader_id = glCreateShader(type);

  const char *shader_source = source.c_str();
  glShaderSource(shader_id, 1, &shader_source, NULL);
  glCompileShader(shader_id);

  int success;
  glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);

  if (!success) {
    std::vector<char> shader_error(1024);
    glGetShaderInfoLog(shader_id, shader_error.size(), nullptr,
                       shader_error.data());
    ASTRA_EXCEPTION(std::string(shader_error.begin(), shader_error.end()));
  }

  return shader_id;
}

uint32_t OpenGLShader::compile(Ref<Path> path, uint32_t type) {
  return compile_glsl(get_file_content_str(path), type);
}

} // namespace astralix
