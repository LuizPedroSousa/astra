#include "serializers/project-serializer.hpp"
#include "assert.hpp"
#include "context-proxy.hpp"
#include "path.hpp"
#include "project.hpp"
#include "resources/font.hpp"
#include "resources/material.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"
#include "serialization-context.hpp"
#include "serializer.hpp"
#include <string_view>

namespace astralix {

inline bool is_set(const std::string &v) { return !v.empty(); }
inline bool is_set(int v) { return v != 0; }
inline bool is_set(bool v) { return true; }

enum class ResourceType {
  Texture2D,
  Texture3D,
  Material,
  Shader,
  Font,
  Unknown
};

Ref<Path> ProjectSerializer::parse_path(ContextProxy ctx) {
  std::string relative_path;
  BaseDirectory base_directory = BaseDirectory::Project;

  switch (ctx.kind()) {

  case SerializationTypeKind::String: {
    auto full_path_str = ctx.as<std::string>();

    ASTRA_ENSURE(full_path_str.empty(), "Path is empty");

    if (full_path_str.starts_with("@")) {
      std::string_view path_str = full_path_str;

      auto segment_position = path_str.find("/", 1);

      ASTRA_ENSURE(segment_position == std::string_view::npos,
                   "Invalid path: ", path_str);

      std::string_view alias_view = path_str.substr(1, segment_position - 1);

      ASTRA_ENSURE(alias_view != "engine" && alias_view != "project",
                   "Unknown base directory: ", alias_view);

      if (alias_view == "engine") {
        base_directory = BaseDirectory::Engine;
      }

      relative_path = full_path_str.substr(alias_view.size() + 2);
      break;
    }

    relative_path = full_path_str;

    break;
  }

  default:
    return nullptr;
  }

  return Path::create(relative_path, base_directory);
}

TextureParameter texture_param_from_string(const std::string &key) {
  if (key == "wrap_s")
    return TextureParameter::WrapS;
  if (key == "wrap_t")
    return TextureParameter::WrapT;
  if (key == "mag_filter")
    return TextureParameter::MagFilter;
  if (key == "min_filter")
    return TextureParameter::MinFilter;

  ASTRA_EXCEPTION("Unknown texture parameter", key);
}

TextureValue texture_value_from_string(const std::string &value) {
  if (value == "repeat")
    return TextureValue::Repeat;
  if (value == "clamp_to_edge")
    return TextureValue::ClampToEdge;
  if (value == "clamp_to_border")
    return TextureValue::ClampToBorder;
  if (value == "linear")
    return TextureValue::Linear;
  if (value == "nearest")
    return TextureValue::Nearest;
  if (value == "linear_mip_map")
    return TextureValue::LinearMipMap;

  ASTRA_EXCEPTION("Unknown texture value", value);
}

ResourceType asset_type_from_string(const std::string &type) {
  if (type == "Texture2D")
    return ResourceType::Texture2D;
  if (type == "Texture3D")
    return ResourceType::Texture3D;
  if (type == "Material")
    return ResourceType::Material;
  if (type == "Shader")
    return ResourceType::Shader;
  if (type == "Font")
    return ResourceType::Font;

  return ResourceType::Unknown;
}

#define SET_CONFIG(value, key)                                                 \
  do {                                                                         \
    auto &&_v = (value);                                                       \
    if (is_set(_v))                                                            \
      config.key = _v;                                                         \
  } while (0)

ProjectSerializer::ProjectSerializer(Ref<Project> project,
                                     Ref<SerializationContext> ctx)
    : Serializer(std::move(ctx)), m_project(project) {}

ProjectSerializer::ProjectSerializer() {}

void ProjectSerializer::serialize() {
  SerializationContext &ctx = *m_ctx.get();

  auto &config = m_project->get_config();

  ctx["config"]["name"] = config.name;
  ctx["config"]["directory"] = config.directory;
  ctx["config"]["resources"]["directory"] = config.resources.directory;
}

static SystemType system_type_from_string(const std::string &name) {
  if (name == "physics")
    return SystemType::Physics;
  if (name == "render")
    return SystemType::Render;

  ASTRA_EXCEPTION("Unknown system type:", name);
}

void ProjectSerializer::deserialize() {
  auto &config = m_project->get_config();

  SerializationContext &ctx = *m_ctx.get();

  SET_CONFIG(ctx["project"]["name"].as<std::string>(), name);
  SET_CONFIG(ctx["project"]["directory"].as<std::string>(), directory);
  SET_CONFIG(ctx["project"]["resources"]["directory"].as<std::string>(),
             resources.directory);

  SET_CONFIG(config.serialization.formatFromString(
                 ctx["project"]["serialization"]["format"].as<std::string>()),
             serialization.format);

  auto windows_size = ctx["windows"].size();

  for (int i = 0; i < windows_size; i++) {
    auto current = ctx["windows"][i];

    WindowConfig window{
        .id = current["id"].as<std::string>(),
        .title = current["title"].as<std::string>(),
        .headless = current["headless"].as<bool>(),
        .height = current["height"].as<int>(),
        .width = current["width"].as<int>(),
    };

    config.windows.push_back(window);
  }

  auto resources = ctx["resources"];
  auto resources_size = resources.size();

  for (int i = 0; i < resources_size; i++) {
    auto asset = resources[i];

    auto id = asset["id"].as<std::string>();
    auto type_str = asset["type"].as<std::string>();
    auto asset_type = asset_type_from_string(type_str);

    switch (asset_type) {
    case ResourceType::Font: {
      auto path = parse_path(asset["path"]);

      ASTRA_ENSURE(path == nullptr, "Font path is required");

      Font::create(id, path);
      break;
    }
    case ResourceType::Texture3D: {
      std::vector<Ref<Path>> faces;

      auto face_size = asset["faces"].size();

      for (int j = 0; j < face_size; j++) {
        faces.push_back(parse_path(asset["faces"][j]));
      }

      Texture3D::create(id, faces);
      break;
    }
    case ResourceType::Texture2D: {
      auto path = parse_path(asset["path"]);
      bool flip = asset["flip"].as<bool>();

      std::unordered_map<TextureParameter, TextureValue> parameters;

      auto parameters_size = asset["parameters"].size();

      if (parameters_size > 0) {
        for (int j = 0; j < parameters_size; j++) {
          auto param = asset["parameters"][j];

          auto key = param["key"].as<std::string>();
          auto value = param["value"].as<std::string>();

          parameters.emplace(texture_param_from_string(key),
                             texture_value_from_string(value));
        }
      }

      Texture2D::create(id, path, flip, parameters);
      break;
    }
    case ResourceType::Material: {
      auto textures = asset["textures"];

      std::vector<std::string> diffuse;
      std::vector<std::string> specular;

      auto diffuse_size = textures["diffuse"].size();
      auto specular_size = textures["specular"].size();

      for (int j = 0; j < diffuse_size; j++) {
        diffuse.push_back(textures["diffuse"][j].as<std::string>());
      }

      for (int j = 0; j < specular_size; j++) {
        specular.push_back(textures["specular"][j].as<std::string>());
      }

      std::string normal = textures["normal"].as<std::string>();

      std::string displacement = textures["displacement"].as<std::string>();

      Material::create(id, diffuse, specular, normal, displacement);
      break;
    }
    case ResourceType::Shader: {
      auto vertex = parse_path(asset["vertex"]);
      auto fragment = parse_path(asset["fragment"]);
      auto geometry = parse_path(asset["geometry"]);

      Shader::create(id, fragment, vertex, geometry);

      break;
    }
    default:
      ASTRA_EXCEPTION("Unknown asset type", type_str);
    }
  }

  auto systems_size = ctx["systems"].size();

  for (int i = 0; i < systems_size; i++) {
    auto sys = ctx["systems"][i];
    auto name = sys["name"].as<std::string>();
    auto system_type = system_type_from_string(name);

    switch (system_type) {
    case SystemType::Physics: {

      PhysicsSystemConfig physics;

      physics.backend = sys["content"]["backend"].as<std::string>();
      physics.pvd_host = sys["content"]["pvd"]["host"].as<std::string>();
      physics.pvd_port = sys["content"]["pvd"]["port"].as<int>();
      physics.pvd_timeout = sys["content"]["pvd"]["timeout"].as<int>();

      physics.gravity = {
          sys["content"]["scene"]["gravity"]["x"].as<float>(),
          sys["content"]["scene"]["gravity"]["y"].as<float>(),
          sys["content"]["scene"]["gravity"]["z"].as<float>(),
      };

      config.systems.push_back({
          .name = name,
          .type = system_type,
          .content = physics,
      });

      break;
    }

    case SystemType::Render: {
      RenderSystemConfig render;

      auto content = sys["content"];

      render.backend = content["backend"].as<std::string>();
      render.headless = content["headless"].as<bool>();
      render.window_id = content["window"].as<std::string>();

      auto msaa = content["msaa"];

      render.msaa.samples = msaa["samples"].as<int>();
      render.msaa.is_enabled = msaa["is_enabled"].as<bool>();

      config.systems.push_back({
          .name = name,
          .type = system_type,
          .content = render,
      });
    } break;

    default:
      ASTRA_EXCEPTION("Unkown system type", name)
    }
  }
}

#undef SET_CONFIG

} // namespace astralix
