#include "opengl-shader.hpp"
#include "assert.hpp"
#include "glad/glad.h"
#include "log.hpp"

#include "fstream"
#include "iostream"
#include "managers/path-manager.hpp"
#include "resources/shader.hpp"
#include "sstream"
#include <cstring>

namespace astralix {

OpenGLShader::OpenGLShader(const ResourceHandle &resource_id,
                           Ref<ShaderDescriptor> descriptor)
    : Shader(resource_id, descriptor->id) {
  m_renderer_id = glCreateProgram();

  m_vertex_id = compile(descriptor->vertex_path, GL_VERTEX_SHADER);
  m_fragment_id = compile(descriptor->fragment_path, GL_FRAGMENT_SHADER);

  if (descriptor->geometry_path != nullptr) {
    m_geometry_id = compile(descriptor->geometry_path, GL_GEOMETRY_SHADER);
  }

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
    LOG_DEBUG(m_fragment_id, m_vertex_id, m_geometry_id);
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

uint32_t OpenGLShader::compile(Ref<Path> path, uint32_t type) {
  uint32_t shader_id = glCreateShader(type);

  auto source = get_file_content_str(path);

  const char *shader_source = source.c_str();

  glShaderSource(shader_id, 1, &shader_source, NULL);

  glCompileShader(shader_id);

  int success;
  glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);

  if (!success) {
    std::vector<char> shader_error(1024);
    glGetShaderInfoLog(shader_id, shader_error.size(), nullptr,
                       shader_error.data());

    std::string error = "Can't load shader from " + path->get_relative_path() +
                        "\n" +
                        std::string(shader_error.begin(), shader_error.end());
    ASTRA_EXCEPTION(error);
  };

  return shader_id;
};

} // namespace astralix
