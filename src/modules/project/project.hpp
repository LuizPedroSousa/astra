#pragma once
#include "arena.hpp"
#include "assert.hpp"
#include "base.hpp"
#include "guid.hpp"
#include "renderer-api.hpp"
#include "serialization-context.hpp"
#include "serializers/project-serializer.hpp"
#include "targets/render-target.hpp"
#include <glm/ext/vector_float3.hpp>
#include <string>

namespace astralix {

class ProjectSerializer;

#define RESOURCE_INIT_PARAMS const ResourceHandle &id

struct ProjectSerializationConfig {
  SerializationFormat format;

  SerializationFormat formatFromString(const std::string &format) {
    static const std::map<std::string, SerializationFormat> formats = {
        {"json", SerializationFormat::Json},
    };

    auto it = formats.find(format);

    ASTRA_ENSURE_WITH_SUGGESTIONS(it == formats.end(), formats, format,
                                  "serialization format",
                                  "ProjectSerialization")

    return it->second;
  }
};

struct ProjectResourceConfig {
  std::string directory;
};

struct WindowConfig {
  WindowID id;
  std::string title;
  bool headless;
  int height;
  int width;
};

enum SystemType { Physics, Render };

struct PhysicsSystemConfig {
  std::string backend;
  glm::vec3 gravity;
  std::string pvd_host = "127.0.0.1";
  int pvd_port;
  int pvd_timeout = 5;
};

struct MSAAConfig {
  int samples;
  bool is_enabled;
};

struct RenderSystemConfig {
  std::string backend;
  MSAAConfig msaa;
  std::string window_id;
  bool headless = false;

  RendererBackend backend_to_api() {
    if (backend == "opengl")
      return RendererBackend::OpenGL;

    return RendererBackend::None;
  }

  RenderTarget::MSAA msaa_to_render_target_msaa() {
    return {.samples = msaa.samples, .is_enabled = msaa.is_enabled};
  }
};

struct SystemConfig {
  std::string name;
  SystemType type;
  std::variant<std::monostate, PhysicsSystemConfig, RenderSystemConfig> content;
};

struct ProjectConfig {
  std::string name = "Untitled";
  std::string directory;

  std::string manifest;

  std::vector<WindowConfig> windows;

  std::vector<SystemConfig> systems;

  ProjectResourceConfig resources;
  ProjectSerializationConfig serialization;
};

class Project {
public:
  ProjectID get_project_id() const { return m_project_id; }

  static Ref<Project> create(ProjectConfig config);
  ProjectConfig &get_config() { return m_config; }
  void save(ElasticArena &arena);
  void load(ElasticArena &arena);
  Project(ProjectConfig config);

private:
  ProjectConfig m_config;
  ProjectID m_project_id;
  Scope<ProjectSerializer> m_serializer;
};

} // namespace astralix
