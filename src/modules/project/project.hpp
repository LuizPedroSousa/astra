#pragma once
#include "arena.hpp"
#include "assert.hpp"
#include "base.hpp"
#include "guid.hpp"
#include "renderer-api.hpp"
#include "serialization-context.hpp"
#include "serializers/project-serializer.hpp"
#include "targets/render-target.hpp"
#include <filesystem>
#include <glm/ext/vector_float3.hpp>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace astralix {

class ProjectSerializer;

#define RESOURCE_INIT_PARAMS const ResourceHandle &id

struct ProjectSerializationConfig {
  SerializationFormat format = SerializationFormat::Json;

  SerializationFormat formatFromString(const std::string &format) {
    static const std::map<std::string, SerializationFormat> formats = {
        {"json", SerializationFormat::Json},
        {"toml", SerializationFormat::Toml},
        {"yaml", SerializationFormat::Yaml},
        {"xml", SerializationFormat::Xml},
    };

    auto it = formats.find(format);

    ASTRA_ENSURE_WITH_SUGGESTIONS(it == formats.end(), formats, format, "serialization format", "ProjectSerialization")

    return it->second;
  }
};

struct ProjectResourceConfig {
  std::string directory;
};

struct ProjectSceneEntryConfig {
  std::string id;
  std::string type;
  std::string path;
};

struct ProjectScenesConfig {
  std::string startup;
  std::vector<ProjectSceneEntryConfig> entries;
};

struct WindowConfig {
  WindowID id;
  std::string title;
  bool headless;
  int height;
  int width;
};

enum SystemType {
  Physics,
  Render
};

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
  std::string strategy = "deferred";
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
  ProjectScenesConfig scenes;
};

class Project {
public:
  ProjectID get_project_id() const { return m_project_id; }

  static Ref<Project> create(ProjectConfig config);
  ProjectConfig &get_config() { return m_config; }
  const ProjectConfig &get_config() const { return m_config; }
  void save(ElasticArena &arena);
  void load(ElasticArena &arena);
  std::filesystem::path manifest_path() const;
  std::filesystem::path resolve_path(
      const std::filesystem::path &relative_path
  ) const;
  const ProjectSceneEntryConfig *find_scene_entry(std::string_view id) const;
  Project(ProjectConfig config);

private:
  ProjectConfig m_config;
  ProjectID m_project_id;
  Scope<ProjectSerializer> m_serializer;
};

} // namespace astralix
