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
#include "sstream"
#include <cstring>

namespace astralix {

OpenGLShader::OpenGLShader(const ResourceHandle &resource_id,
                           Ref<ShaderDescriptor> descriptor)
    : Shader(resource_id, descriptor->id) {

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

  std::map<std::string, CompileResult> axsl_results;

  auto ensure_compiled = [&](const std::filesystem::path &resolved) {
    if (resolved.extension() != ".axsl") {
      return;
    }

    auto key = resolved.string();

    if (axsl_results.count(key)) {
      return;
    }

    auto source_mtime = std::filesystem::last_write_time(resolved);
    bool any_exists = false;
    bool any_stale = false;

    for (auto [kind, ext] : stage_extensions) {
      auto cached = cache_path(resolved, ext);
      if (!std::filesystem::exists(cached))
        continue;
      any_exists = true;
      if (std::filesystem::last_write_time(cached) < source_mtime) {
        any_stale = true;
        break;
      }
    }

    if (any_exists && !any_stale) {
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
      axsl_results[key] = std::move(cached_result);
      return;
    }

    std::ifstream file(resolved);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    auto result = compiler.compile(
        buffer.str(), resolved.parent_path().string(), resolved.string());

    if (result.ok()) {
      for (auto [kind, ext] : stage_extensions) {
        auto it = result.stages.find(kind);

        if (it == result.stages.end()) {
          continue;
        }
        std::ofstream out(cache_path(resolved, ext));
        out << it->second;
      }
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

void OpenGLShader::set_bool(const std::string &name, bool value) const {

  glUniform1i(glGetUniformLocation(m_renderer_id, name.c_str()), (int)value);
}
void OpenGLShader::set_int(const std::string &name, int value) const {

  glUniform1i(glGetUniformLocation(m_renderer_id, name.c_str()), (int)value);
}
void OpenGLShader::set_matrix(const std::string &name, glm::mat4 matrix) const {

  glUniformMatrix4fv(glGetUniformLocation(m_renderer_id, name.c_str()), 1,
                     GL_FALSE, glm::value_ptr(matrix));
}
void OpenGLShader::set_float(const std::string &name, float value) const {

  glUniform1f(glGetUniformLocation(m_renderer_id, name.c_str()), value);
}
void OpenGLShader::set_vec3(const std::string &name, glm::vec3 value) const {

  glUniform3f(glGetUniformLocation(m_renderer_id, name.c_str()), value.x,
              value.y, value.z);
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
