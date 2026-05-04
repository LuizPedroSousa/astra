#include "serializers/project-serializer.hpp"
#include "assert.hpp"
#include "context-proxy.hpp"
#include "path.hpp"
#include "project.hpp"
#include "resources/audio-clip.hpp"
#include "resources/font.hpp"
#include "resources/material.hpp"
#include "resources/model.hpp"
#include "resources/shader.hpp"
#include "resources/svg.hpp"
#include "resources/terrain-recipe.hpp"
#include "resources/texture.hpp"
#include "serialization-context.hpp"
#include "serializer.hpp"
#include <glm/glm.hpp>
#include <string_view>
#include <unordered_set>

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
  Model,
  Svg,
  AudioClip,
  TerrainRecipe,
  Unknown
};

bool has_inline_resource_fields(ContextProxy &asset) {
  return asset["type"].kind() == SerializationTypeKind::String ||
         asset["path"].kind() == SerializationTypeKind::String ||
         asset["vertex"].kind() == SerializationTypeKind::String ||
         asset["fragment"].kind() == SerializationTypeKind::String ||
         asset["compute"].kind() == SerializationTypeKind::String ||
         asset["geometry"].kind() == SerializationTypeKind::String ||
         asset["faces"].kind() == SerializationTypeKind::Array ||
         asset["equirectangular"].kind() == SerializationTypeKind::String ||
         asset["textures"].kind() == SerializationTypeKind::Object;
}

bool is_asset_binding_entry(ContextProxy &asset) {
  return asset["asset"].kind() == SerializationTypeKind::String;
}

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

        ASTRA_ENSURE(segment_position == std::string_view::npos, "Invalid path: ", path_str);

        std::string_view alias_view = path_str.substr(1, segment_position - 1);

        ASTRA_ENSURE(alias_view != "engine" && alias_view != "project", "Unknown base directory: ", alias_view);

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

float read_number(ContextProxy ctx, float fallback = 0.0f) {
  switch (ctx.kind()) {
    case SerializationTypeKind::Float:
      return ctx.as<float>();
    case SerializationTypeKind::Int:
      return static_cast<float>(ctx.as<int>());
    default:
      return fallback;
  }
}

glm::vec3 read_vec3(ContextProxy ctx, const glm::vec3 &fallback = glm::vec3(0.0f)) {
  if (ctx.kind() != SerializationTypeKind::Object) {
    return fallback;
  }

  return glm::vec3(read_number(ctx["x"], fallback.x), read_number(ctx["y"], fallback.y), read_number(ctx["z"], fallback.z));
}

glm::vec4 read_vec4(ContextProxy ctx, const glm::vec4 &fallback = glm::vec4(0.0f)) {
  if (ctx.kind() != SerializationTypeKind::Object) {
    return fallback;
  }

  return glm::vec4(
      read_number(ctx["x"], fallback.x),
      read_number(ctx["y"], fallback.y),
      read_number(ctx["z"], fallback.z),
      read_number(ctx["w"], fallback.w)
  );
}

bool read_bool(ContextProxy ctx, bool fallback = false) {
  if (ctx.kind() == SerializationTypeKind::Bool) {
    return ctx.as<bool>();
  }

  return fallback;
}

std::optional<std::string> read_optional_string(ContextProxy ctx) {
  if (ctx.kind() != SerializationTypeKind::String) {
    return std::nullopt;
  }

  return ctx.as<std::string>();
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
  if (type == "Model")
    return ResourceType::Model;
  if (type == "Svg")
    return ResourceType::Svg;
  if (type == "AudioClip")
    return ResourceType::AudioClip;
  if (type == "TerrainRecipe")
    return ResourceType::TerrainRecipe;

  return ResourceType::Unknown;
}

std::string serialization_format_to_string(SerializationFormat format) {
  switch (format) {
    case SerializationFormat::Json:
      return "json";
    case SerializationFormat::Yaml:
      return "yaml";
    case SerializationFormat::Toml:
      return "toml";
    case SerializationFormat::Xml:
      return "xml";
    default:
      ASTRA_EXCEPTION("Unknown serialization format");
  }
}

std::string scene_startup_target_to_string(SceneStartupTarget target) {
  switch (target) {
    case SceneStartupTarget::Source:
      return "source";
    case SceneStartupTarget::Preview:
      return "preview";
    case SceneStartupTarget::Runtime:
      return "runtime";
    default:
      ASTRA_EXCEPTION("Unknown scene startup target");
  }
}

SceneStartupTarget scene_startup_target_from_string(const std::string &value) {
  if (value == "source") {
    return SceneStartupTarget::Source;
  }

  if (value == "preview") {
    return SceneStartupTarget::Preview;
  }

  if (value == "runtime") {
    return SceneStartupTarget::Runtime;
  }

  ASTRA_EXCEPTION("Unknown scene startup target: ", value);
}

RenderGraphPassDependencyKind
render_graph_dependency_kind_from_section(std::string_view section) {
  if (section == "shaders") {
    return RenderGraphPassDependencyKind::Shader;
  }

  if (section == "textures" || section == "textures_2d") {
    return RenderGraphPassDependencyKind::Texture2D;
  }

  if (section == "textures_3d" || section == "cubemaps") {
    return RenderGraphPassDependencyKind::Texture3D;
  }

  if (section == "materials") {
    return RenderGraphPassDependencyKind::Material;
  }

  if (section == "models") {
    return RenderGraphPassDependencyKind::Model;
  }

  if (section == "fonts") {
    return RenderGraphPassDependencyKind::Font;
  }

  if (section == "svgs") {
    return RenderGraphPassDependencyKind::Svg;
  }

  if (section == "audio_clips") {
    return RenderGraphPassDependencyKind::AudioClip;
  }

  if (section == "terrain_recipes") {
    return RenderGraphPassDependencyKind::TerrainRecipe;
  }

  ASTRA_EXCEPTION("Unknown render graph dependency section: ", section);
}

std::vector<std::string> read_string_array(ContextProxy ctx) {
  std::vector<std::string> values;
  if (ctx.kind() != SerializationTypeKind::Array) {
    return values;
  }

  const auto count = ctx.size();
  values.reserve(count);

  for (int i = 0; i < count; ++i) {
    auto item = ctx[i];
    if (item.kind() == SerializationTypeKind::String) {
      values.push_back(item.as<std::string>());
    }
  }

  return values;
}

RenderGraphSizeConfig parse_render_graph_size(ContextProxy ctx) {
  RenderGraphSizeConfig size;

  switch (ctx.kind()) {
    case SerializationTypeKind::String: {
      const auto token = ctx.as<std::string>();
      ASTRA_ENSURE(
          token != "window",
          "Unsupported render graph size token: ",
          token,
          ". Use \"window\" or an operation object."
      );

      size.mode = RenderGraphSizeMode::WindowRelative;
      size.scale_x = 1.0f;
      size.scale_y = 1.0f;
      size.defined = true;
      return size;
    }

    case SerializationTypeKind::Array: {
      ASTRA_ENSURE(
          ctx.size() < 2,
          "Render graph absolute size arrays require [width, height]"
      );

      size.mode = RenderGraphSizeMode::Absolute;
      size.width = static_cast<uint32_t>(read_number(ctx[0]));
      size.height = static_cast<uint32_t>(read_number(ctx[1]));
      size.defined = true;

      ASTRA_ENSURE(
          size.width == 0 || size.height == 0,
          "Render graph absolute size must be greater than zero"
      );

      return size;
    }

    case SerializationTypeKind::Object: {
      const auto width = static_cast<uint32_t>(read_number(ctx["width"]));
      const auto height = static_cast<uint32_t>(read_number(ctx["height"]));
      if (width > 0 && height > 0) {
        size.mode = RenderGraphSizeMode::Absolute;
        size.width = width;
        size.height = height;
        size.defined = true;
        return size;
      }

      auto source_ctx = ctx["source"];
      ASTRA_ENSURE(
          source_ctx.kind() != SerializationTypeKind::String,
          "Render graph size object requires either width/height or source"
      );

      const auto source = source_ctx.as<std::string>();
      ASTRA_ENSURE(
          source != "window",
          "Unsupported render graph size source: ",
          source
      );

      size.mode = RenderGraphSizeMode::WindowRelative;
      size.scale_x = 1.0f;
      size.scale_y = 1.0f;
      size.defined = true;

      auto op_ctx = ctx["op"];
      if (op_ctx.kind() != SerializationTypeKind::String) {
        return size;
      }

      const auto op = op_ctx.as<std::string>();
      float scalar = read_number(ctx["value"], 1.0f);
      float x = read_number(ctx["x"], scalar);
      float y = read_number(ctx["y"], scalar);

      ASTRA_ENSURE(
          x <= 0.0f || y <= 0.0f,
          "Render graph size operations require positive x/y values"
      );

      if (op == "multiply" || op == "scale") {
        size.scale_x *= x;
        size.scale_y *= y;
        return size;
      }

      if (op == "divide") {
        size.scale_x /= x;
        size.scale_y /= y;
        return size;
      }

      ASTRA_EXCEPTION("Unknown render graph size operation: ", op);
    }

    default:
      break;
  }

  return size;
}

RenderGraphConfig parse_render_graph(ContextProxy ctx) {
  RenderGraphConfig render_graph;
  if (ctx.kind() != SerializationTypeKind::Object) {
    return render_graph;
  }

  auto resources_ctx = ctx["resources"];
  if (resources_ctx.kind() == SerializationTypeKind::Object) {
    for (const auto &resource_name : resources_ctx.object_keys()) {
      auto resource_ctx = resources_ctx[resource_name];

      RenderGraphResourceConfig resource;
      resource.name = resource_name;
      if (resource_ctx["format"].kind() == SerializationTypeKind::String) {
        resource.format = resource_ctx["format"].as<std::string>();
      }
      resource.size = parse_render_graph_size(resource_ctx["size"]);
      resource.usage = read_string_array(resource_ctx["usage"]);
      if (resource_ctx["lifetime"].kind() == SerializationTypeKind::String) {
        resource.lifetime = resource_ctx["lifetime"].as<std::string>();
      }

      ASTRA_ENSURE(
          resource.format.empty(),
          "Render graph resource format is required for: ",
          resource.name
      );
      ASTRA_ENSURE(
          !resource.size.defined,
          "Render graph size is required for resource: ",
          resource.name
      );
      ASTRA_ENSURE(
          resource.usage.empty(),
          "Render graph usage list is required for resource: ",
          resource.name
      );

      render_graph.resources.push_back(std::move(resource));
    }
  }

  auto passes_ctx = ctx["passes"];
  if (passes_ctx.kind() == SerializationTypeKind::Array) {
    const auto pass_count = passes_ctx.size();
    render_graph.passes.reserve(pass_count);

    for (int pass_index = 0; pass_index < pass_count; ++pass_index) {
      auto pass_ctx = passes_ctx[pass_index];

      RenderGraphPassConfig pass;
      if (pass_ctx["id"].kind() == SerializationTypeKind::String) {
        pass.id = pass_ctx["id"].as<std::string>();
      }
      if (pass_ctx["type"].kind() == SerializationTypeKind::String) {
        pass.type = pass_ctx["type"].as<std::string>();
      }

      auto dependencies_ctx = pass_ctx["dependencies"];
      if (dependencies_ctx.kind() == SerializationTypeKind::Object) {
        for (const auto &section : dependencies_ctx.object_keys()) {
          auto section_ctx = dependencies_ctx[section];
          if (section_ctx.kind() != SerializationTypeKind::Object) {
            continue;
          }

          const auto kind =
              render_graph_dependency_kind_from_section(section);
          for (const auto &slot : section_ctx.object_keys()) {
            auto descriptor_ctx = section_ctx[slot];
            if (descriptor_ctx.kind() != SerializationTypeKind::String) {
              continue;
            }

            pass.dependencies.push_back(RenderGraphPassDependencyConfig{
                .kind = kind,
                .slot = slot,
                .descriptor_id = descriptor_ctx.as<std::string>(),
            });
          }
        }
      }

      auto use_ctx = pass_ctx["use"];
      if (use_ctx.kind() == SerializationTypeKind::Array) {
        const auto use_count = use_ctx.size();
        pass.uses.reserve(use_count);

        for (int use_index = 0; use_index < use_count; ++use_index) {
          auto current_use = use_ctx[use_index];
          if (current_use.kind() != SerializationTypeKind::Object) {
            continue;
          }

          RenderGraphPassUseConfig use;
          if (current_use["resource"].kind() == SerializationTypeKind::String) {
            use.resource = current_use["resource"].as<std::string>();
          }
          if (current_use["aspect"].kind() == SerializationTypeKind::String) {
            use.aspect = current_use["aspect"].as<std::string>();
          }
          if (current_use["usage"].kind() == SerializationTypeKind::String) {
            use.usage = current_use["usage"].as<std::string>();
          }

          ASTRA_ENSURE(
              use.resource.empty() || use.aspect.empty() || use.usage.empty(),
              "Render graph pass use entries require resource, aspect, and usage"
          );

          pass.uses.push_back(std::move(use));
        }
      }

      auto present_ctx = pass_ctx["present"];
      if (present_ctx.kind() == SerializationTypeKind::Object) {
        RenderGraphPassPresentConfig present;
        if (present_ctx["resource"].kind() == SerializationTypeKind::String) {
          present.resource = present_ctx["resource"].as<std::string>();
        }
        if (present_ctx["aspect"].kind() == SerializationTypeKind::String) {
          present.aspect = present_ctx["aspect"].as<std::string>();
        }

        ASTRA_ENSURE(
            present.resource.empty(),
            "Render graph present entries require a resource"
        );

        pass.present = std::move(present);
      }

      ASTRA_ENSURE(
          pass.type.empty(),
          "Render graph pass type is required"
      );

      render_graph.passes.push_back(std::move(pass));
    }
  }

  return render_graph;
}

void deserialize_inline_resource(ProjectSerializer &serializer, ContextProxy &asset) {
  auto id = asset["id"].as<std::string>();
  auto type_str = asset["type"].as<std::string>();
  auto asset_type = asset_type_from_string(type_str);

  switch (asset_type) {
  case ResourceType::Font: {
    auto path = serializer.parse_path(asset["path"]);

    ASTRA_ENSURE(path == nullptr, "Font path is required");

    Font::create(id, path);
    break;
  }
  case ResourceType::Texture3D: {
    if (asset["equirectangular"].kind() == SerializationTypeKind::String) {
      auto equirectangular_path = serializer.parse_path(asset["equirectangular"]);
      Texture3D::create_from_equirectangular(id, equirectangular_path);
    } else {
      std::vector<Ref<Path>> faces;
      auto face_size = asset["faces"].size();
      for (int j = 0; j < face_size; j++) {
        faces.push_back(serializer.parse_path(asset["faces"][j]));
      }
      Texture3D::create(id, faces);
    }
    break;
  }
  case ResourceType::Texture2D: {
    auto path = serializer.parse_path(asset["path"]);
    bool flip = asset["flip"].as<bool>();

    std::unordered_map<TextureParameter, TextureValue> parameters;

    auto parameters_size = asset["parameters"].size();

    if (parameters_size > 0) {
      for (int j = 0; j < parameters_size; j++) {
        auto param = asset["parameters"][j];

        auto key = param["key"].as<std::string>();
        auto value = param["value"].as<std::string>();

        parameters.emplace(
            texture_param_from_string(key),
            texture_value_from_string(value)
        );
      }
    }

    Texture2D::create(id, path, flip, parameters);
    break;
  }
  case ResourceType::Material: {
    ASTRA_ENSURE(
        asset["textures"]["diffuse"].kind() !=
                SerializationTypeKind::Unknown ||
            asset["textures"]["specular"].kind() !=
                SerializationTypeKind::Unknown,
        "Material '",
        id,
        "' still uses classic material keys"
    );
    ASTRA_ENSURE(
        asset["emissive"].kind() != SerializationTypeKind::Unknown,
        "Material '",
        id,
        "' still uses classic material fields"
    );

    auto textures = asset["textures"];

    auto read_optional_id = [](ContextProxy ctx)
        -> std::optional<ResourceDescriptorID> {
      if (ctx.kind() != SerializationTypeKind::String) {
        return std::nullopt;
      }

      const auto value = ctx.as<std::string>();
      if (value.empty()) {
        return std::nullopt;
      }

      return value;
    };

    Material::create(
        id,
        read_optional_id(textures["base_color"]),
        read_optional_id(textures["normal"]),
        read_optional_id(textures["metallic"]),
        read_optional_id(textures["roughness"]),
        read_optional_id(textures["metallic_roughness"]),
        read_optional_id(textures["occlusion"]),
        read_optional_id(textures["emissive"]),
        read_optional_id(textures["displacement"]),
        read_vec4(asset["base_color_factor"], glm::vec4(1.0f)),
        read_vec3(asset["emissive_factor"], glm::vec3(0.0f)),
        read_number(asset["metallic_factor"], 1.0f),
        read_number(asset["roughness_factor"], 1.0f),
        read_number(asset["occlusion_strength"], 1.0f),
        read_number(asset["normal_scale"], 1.0f),
        read_number(asset["height_scale"], 0.02f),
        read_number(asset["bloom_intensity"], 0.0f),
        [&]() {
          auto alpha_mode = read_optional_string(asset["alpha_mode"]);
          if (alpha_mode.has_value()) {
            const auto normalized = to_lower(*alpha_mode);
            ASTRA_ENSURE(
                normalized != "opaque" && normalized != "mask" &&
                    normalized != "blend",
                "Unsupported material alpha_mode: ",
                *alpha_mode
            );
            return normalized == "mask";
          }

          return read_bool(asset["alpha_mask"], false);
        }(),
        [&]() {
          auto alpha_mode = read_optional_string(asset["alpha_mode"]);
          if (alpha_mode.has_value()) {
            return to_lower(*alpha_mode) == "blend";
          }
          return read_bool(asset["alpha_blend"], false);
        }(),
        read_number(asset["alpha_cutoff"], 0.5f),
        read_bool(asset["double_sided"], false)
    );
    break;
  }
  case ResourceType::Shader: {
    auto vertex = serializer.parse_path(asset["vertex"]);
    auto fragment = serializer.parse_path(asset["fragment"]);
    auto geometry = serializer.parse_path(asset["geometry"]);
    auto compute = serializer.parse_path(asset["compute"]);

    Shader::create(id, fragment, vertex, geometry, compute);

    break;
  }
  case ResourceType::Model: {
    auto path = serializer.parse_path(asset["path"]);

    ASTRA_ENSURE(path == nullptr, "Model path is required");

    Model::create(id, path);
    break;
  }
  case ResourceType::Svg: {
    auto path = serializer.parse_path(asset["path"]);

    ASTRA_ENSURE(path == nullptr, "SVG path is required");

    Svg::create(id, path);
    break;
  }
  case ResourceType::AudioClip: {
    auto path = serializer.parse_path(asset["path"]);

    ASTRA_ENSURE(path == nullptr, "AudioClip path is required");

    AudioClip::create(id, path);
    break;
  }
  case ResourceType::TerrainRecipe: {
    auto path = serializer.parse_path(asset["path"]);

    ASTRA_ENSURE(path == nullptr, "TerrainRecipe path is required");

    TerrainRecipe::create(id, path);
    break;
  }
  default: {
    ExceptionMetadata metadata;

    metadata.push_back({"id", id});
    metadata.push_back({"type", type_str});

    ASTRA_EXCEPTION_META(metadata, "Unknown asset type", type_str);
  }
  }
}

#define SET_CONFIG(value, key) \
  do {                         \
    auto &&_v = (value);       \
    if (is_set(_v))            \
      config.key = _v;         \
  } while (0)

ProjectSerializer::ProjectSerializer(Ref<Project> project, Ref<SerializationContext> ctx)
    : Serializer(std::move(ctx)), m_project(project) {}

ProjectSerializer::ProjectSerializer() {}

void ProjectSerializer::serialize() {
  SerializationContext &ctx = *m_ctx.get();

  auto &config = m_project->get_config();

  ctx["project"]["name"] = config.name;
  ctx["project"]["directory"] = config.directory;
  ctx["project"]["resources"]["directory"] = config.resources.directory;
  ctx["project"]["serialization"]["format"] =
      serialization_format_to_string(config.serialization.format);
  ctx["project"]["scenes"]["startup"] = config.scenes.startup;
  ctx["project"]["scenes"]["startup_target"] =
      scene_startup_target_to_string(config.scenes.startup_target);

  for (size_t index = 0; index < config.scenes.entries.size(); ++index) {
    const auto &entry = config.scenes.entries[index];
    auto scene_ctx =
        ctx["project"]["scenes"]["entries"][static_cast<int>(index)];
    scene_ctx["id"] = entry.id;
    scene_ctx["type"] = entry.type;
    scene_ctx["source_path"] = entry.source_path;
    scene_ctx["preview_path"] = entry.preview_path;
    scene_ctx["runtime_path"] = entry.runtime_path;
  }
}

static SystemType system_type_from_string(const std::string &name) {
  if (name == "physics")
    return SystemType::Physics;
  if (name == "render")
    return SystemType::Render;
  if (name == "audio")
    return SystemType::Audio;
  if (name == "terrain")
    return SystemType::Terrain;

  ASTRA_EXCEPTION("Unknown system type:", name);
}

void ProjectSerializer::deserialize() {
  auto &config = m_project->get_config();

  SerializationContext &ctx = *m_ctx.get();
  const RenderGraphConfig manifest_render_graph =
      parse_render_graph(ctx["render_graph"]);

  config.windows.clear();
  config.systems.clear();
  config.scenes.entries.clear();
  config.resources.asset_bindings.clear();

  SET_CONFIG(ctx["project"]["name"].as<std::string>(), name);
  SET_CONFIG(ctx["project"]["directory"].as<std::string>(), directory);
  SET_CONFIG(ctx["project"]["resources"]["directory"].as<std::string>(), resources.directory);

  SET_CONFIG(config.serialization.formatFromString(ctx["project"]["serialization"]["format"].as<std::string>()), serialization.format);

  if (ctx["project"]["scenes"]["startup"].kind() == SerializationTypeKind::String) {
    config.scenes.startup =
        ctx["project"]["scenes"]["startup"].as<std::string>();
  }

  ASTRA_ENSURE(
      ctx["project"]["scenes"]["startup_target"].kind() !=
          SerializationTypeKind::String,
      "Scene startup target is required"
  );
  config.scenes.startup_target = scene_startup_target_from_string(
      ctx["project"]["scenes"]["startup_target"].as<std::string>()
  );

  std::unordered_set<std::string> scene_ids;
  const auto scene_entries_size = ctx["project"]["scenes"]["entries"].size();
  config.scenes.entries.reserve(scene_entries_size);

  for (int i = 0; i < scene_entries_size; i++) {
    auto entry_ctx = ctx["project"]["scenes"]["entries"][i];

    if (entry_ctx["id"].kind() != SerializationTypeKind::String ||
        entry_ctx["type"].kind() != SerializationTypeKind::String) {
      continue;
    }

    ProjectSceneEntryConfig entry{
        .id = entry_ctx["id"].as<std::string>(),
        .type = entry_ctx["type"].as<std::string>(),
        .source_path =
            entry_ctx["source_path"].kind() == SerializationTypeKind::String
                ? entry_ctx["source_path"].as<std::string>()
                : std::string{},
        .preview_path =
            entry_ctx["preview_path"].kind() == SerializationTypeKind::String
                ? entry_ctx["preview_path"].as<std::string>()
                : std::string{},
        .runtime_path =
            entry_ctx["runtime_path"].kind() == SerializationTypeKind::String
                ? entry_ctx["runtime_path"].as<std::string>()
                : std::string{},
    };

    ASTRA_ENSURE(entry.id.empty(), "Scene entry id is required");
    ASTRA_ENSURE(entry.type.empty(), "Scene entry type is required");
    ASTRA_ENSURE(entry.source_path.empty(), "Scene entry source path is required");
    ASTRA_ENSURE(entry.preview_path.empty(), "Scene entry preview path is required");
    ASTRA_ENSURE(entry.runtime_path.empty(), "Scene entry runtime path is required");
    ASTRA_ENSURE(!scene_ids.insert(entry.id).second, "Duplicate scene entry id: ", entry.id);

    config.scenes.entries.push_back(std::move(entry));
  }

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
    const bool asset_binding = is_asset_binding_entry(asset);
    const bool inline_resource = has_inline_resource_fields(asset);

    ASTRA_ENSURE(
        asset_binding && inline_resource,
        "Resource entry mixes inline fields with an asset binding"
    );

    if (asset_binding) {
      auto id = asset["id"].as<std::string>();
      ASTRA_ENSURE(id.empty(), "Asset binding id is required");

      config.resources.asset_bindings.push_back(AssetBindingConfig{
          .id = std::move(id),
          .asset_path =
              asset["asset"].kind() == SerializationTypeKind::String
                  ? asset["asset"].as<std::string>()
                  : std::string{},
      });

      ASTRA_ENSURE(
          config.resources.asset_bindings.back().asset_path.empty(),
          "Asset binding path is required for ",
          config.resources.asset_bindings.back().id
      );
      continue;
    }

    if (inline_resource) {
      deserialize_inline_resource(*this, asset);
      continue;
    }

    ASTRA_EXCEPTION(
        "Resource entry must declare either inline fields or an asset binding"
    );
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
        if (content["strategy"].kind() == SerializationTypeKind::String) {
          render.strategy = content["strategy"].as<std::string>();
        }
        render.headless = content["headless"].as<bool>();
        render.window_id = content["window"].as<std::string>();
        render.render_graph = manifest_render_graph;

        auto ssao = content["ssao"];
        if (ssao.kind() == SerializationTypeKind::Object &&
            ssao["full_resolution"].kind() == SerializationTypeKind::Bool) {
          render.ssao.full_resolution = ssao["full_resolution"].as<bool>();
        }

        auto ssgi = content["ssgi"];
        if (ssgi.kind() == SerializationTypeKind::Object) {
          render.ssgi.enabled =
              read_bool(ssgi["enabled"], render.ssgi.enabled);
          render.ssgi.full_resolution = read_bool(
              ssgi["full_resolution"], render.ssgi.full_resolution
          );
          render.ssgi.intensity =
              read_number(ssgi["intensity"], render.ssgi.intensity);
          render.ssgi.radius =
              read_number(ssgi["radius"], render.ssgi.radius);
          render.ssgi.thickness =
              read_number(ssgi["thickness"], render.ssgi.thickness);
          render.ssgi.directions = static_cast<int>(
              read_number(
                  ssgi["directions"],
                  static_cast<float>(render.ssgi.directions)
              )
          );
          render.ssgi.steps_per_direction = static_cast<int>(
              read_number(
                  ssgi["steps_per_direction"],
                  static_cast<float>(render.ssgi.steps_per_direction)
              )
          );
          render.ssgi.max_distance = read_number(
              ssgi["max_distance"], render.ssgi.max_distance
          );
          render.ssgi.temporal =
              read_bool(ssgi["temporal"], render.ssgi.temporal);
          render.ssgi.history_weight = read_number(
              ssgi["history_weight"], render.ssgi.history_weight
          );
          render.ssgi.normal_reject_dot = read_number(
              ssgi["normal_reject_dot"],
              render.ssgi.normal_reject_dot
          );
          render.ssgi.position_reject_distance = read_number(
              ssgi["position_reject_distance"],
              render.ssgi.position_reject_distance
          );
        }

        auto ssr = content["ssr"];
        if (ssr.kind() == SerializationTypeKind::Object) {
          render.ssr.enabled =
              read_bool(ssr["enabled"], render.ssr.enabled);
          render.ssr.intensity =
              read_number(ssr["intensity"], render.ssr.intensity);
          render.ssr.max_distance =
              read_number(ssr["max_distance"], render.ssr.max_distance);
          render.ssr.thickness =
              read_number(ssr["thickness"], render.ssr.thickness);
          render.ssr.max_steps = static_cast<int>(
              read_number(
                  ssr["max_steps"],
                  static_cast<float>(render.ssr.max_steps)
              )
          );
          render.ssr.stride =
              read_number(ssr["stride"], render.ssr.stride);
          render.ssr.roughness_cutoff =
              read_number(ssr["roughness_cutoff"], render.ssr.roughness_cutoff);
        }

        auto volumetric = content["volumetric"];
        if (volumetric.kind() == SerializationTypeKind::Object) {
          render.volumetric.enabled =
              read_bool(volumetric["enabled"], render.volumetric.enabled);
          render.volumetric.max_steps = static_cast<int>(
              read_number(
                  volumetric["max_steps"],
                  static_cast<float>(render.volumetric.max_steps)
              )
          );
          render.volumetric.density =
              read_number(volumetric["density"], render.volumetric.density);
          render.volumetric.scattering =
              read_number(volumetric["scattering"], render.volumetric.scattering);
          render.volumetric.max_distance = read_number(
              volumetric["max_distance"], render.volumetric.max_distance
          );
          render.volumetric.intensity =
              read_number(volumetric["intensity"], render.volumetric.intensity);
          render.volumetric.fog_base_height = read_number(
              volumetric["fog_base_height"], render.volumetric.fog_base_height
          );
          render.volumetric.height_falloff_rate = read_number(
              volumetric["height_falloff_rate"],
              render.volumetric.height_falloff_rate
          );
          render.volumetric.noise_scale = read_number(
              volumetric["noise_scale"], render.volumetric.noise_scale
          );
          render.volumetric.noise_weight = read_number(
              volumetric["noise_weight"], render.volumetric.noise_weight
          );
          render.volumetric.wind_speed = read_number(
              volumetric["wind_speed"], render.volumetric.wind_speed
          );
          render.volumetric.temporal =
              read_bool(volumetric["temporal"], render.volumetric.temporal);
          render.volumetric.temporal_blend_weight = read_number(
              volumetric["temporal_blend_weight"],
              render.volumetric.temporal_blend_weight
          );

          auto wind = volumetric["wind_direction"];
          if (wind.kind() == SerializationTypeKind::Array) {
            render.volumetric.wind_direction = glm::vec3(
                read_number(wind[0], 1.0f),
                read_number(wind[1], 0.0f),
                read_number(wind[2], 0.0f)
            );
          }
        }

        auto lens_flare = content["lens_flare"];
        if (lens_flare.kind() == SerializationTypeKind::Object) {
          render.lens_flare.enabled =
              read_bool(lens_flare["enabled"], render.lens_flare.enabled);
          render.lens_flare.intensity =
              read_number(lens_flare["intensity"], render.lens_flare.intensity);
          render.lens_flare.threshold =
              read_number(lens_flare["threshold"], render.lens_flare.threshold);
          render.lens_flare.ghost_count = static_cast<int>(
              read_number(
                  lens_flare["ghost_count"],
                  static_cast<float>(render.lens_flare.ghost_count)
              )
          );
          render.lens_flare.ghost_dispersal = read_number(
              lens_flare["ghost_dispersal"], render.lens_flare.ghost_dispersal
          );
          render.lens_flare.ghost_weight = read_number(
              lens_flare["ghost_weight"], render.lens_flare.ghost_weight
          );
          render.lens_flare.halo_radius = read_number(
              lens_flare["halo_radius"], render.lens_flare.halo_radius
          );
          render.lens_flare.halo_weight = read_number(
              lens_flare["halo_weight"], render.lens_flare.halo_weight
          );
          render.lens_flare.halo_thickness = read_number(
              lens_flare["halo_thickness"], render.lens_flare.halo_thickness
          );
          render.lens_flare.chromatic_aberration = read_number(
              lens_flare["chromatic_aberration"],
              render.lens_flare.chromatic_aberration
          );
        }

        auto eye_adaptation = content["eye_adaptation"];
        if (eye_adaptation.kind() == SerializationTypeKind::Object) {
          render.eye_adaptation.enabled = read_bool(
              eye_adaptation["enabled"], render.eye_adaptation.enabled
          );
          render.eye_adaptation.min_log_luminance = read_number(
              eye_adaptation["min_log_luminance"],
              render.eye_adaptation.min_log_luminance
          );
          render.eye_adaptation.max_log_luminance = read_number(
              eye_adaptation["max_log_luminance"],
              render.eye_adaptation.max_log_luminance
          );
          render.eye_adaptation.adaptation_speed_up = read_number(
              eye_adaptation["adaptation_speed_up"],
              render.eye_adaptation.adaptation_speed_up
          );
          render.eye_adaptation.adaptation_speed_down = read_number(
              eye_adaptation["adaptation_speed_down"],
              render.eye_adaptation.adaptation_speed_down
          );
          render.eye_adaptation.key_value = read_number(
              eye_adaptation["key_value"], render.eye_adaptation.key_value
          );
          render.eye_adaptation.low_percentile = read_number(
              eye_adaptation["low_percentile"],
              render.eye_adaptation.low_percentile
          );
          render.eye_adaptation.high_percentile = read_number(
              eye_adaptation["high_percentile"],
              render.eye_adaptation.high_percentile
          );
        }

        auto motion_blur = content["motion_blur"];
        if (motion_blur.kind() == SerializationTypeKind::Object) {
          render.motion_blur.enabled =
              read_bool(motion_blur["enabled"], render.motion_blur.enabled);
          render.motion_blur.intensity = read_number(
              motion_blur["intensity"], render.motion_blur.intensity
          );
          render.motion_blur.max_samples = static_cast<int>(read_number(
              motion_blur["max_samples"],
              static_cast<float>(render.motion_blur.max_samples)
          ));
          render.motion_blur.depth_threshold = read_number(
              motion_blur["depth_threshold"],
              render.motion_blur.depth_threshold
          );
        }

        auto chromatic_aberration = content["chromatic_aberration"];
        if (chromatic_aberration.kind() == SerializationTypeKind::Object) {
          render.chromatic_aberration.enabled = read_bool(
              chromatic_aberration["enabled"],
              render.chromatic_aberration.enabled
          );
          render.chromatic_aberration.intensity = read_number(
              chromatic_aberration["intensity"],
              render.chromatic_aberration.intensity
          );
        }

        auto vignette = content["vignette"];
        if (vignette.kind() == SerializationTypeKind::Object) {
          render.vignette.enabled =
              read_bool(vignette["enabled"], render.vignette.enabled);
          render.vignette.intensity =
              read_number(vignette["intensity"], render.vignette.intensity);
          render.vignette.smoothness =
              read_number(vignette["smoothness"], render.vignette.smoothness);
          render.vignette.roundness =
              read_number(vignette["roundness"], render.vignette.roundness);
        }

        auto film_grain = content["film_grain"];
        if (film_grain.kind() == SerializationTypeKind::Object) {
          render.film_grain.enabled =
              read_bool(film_grain["enabled"], render.film_grain.enabled);
          render.film_grain.intensity =
              read_number(film_grain["intensity"], render.film_grain.intensity);
        }

        auto depth_of_field = content["depth_of_field"];
        if (depth_of_field.kind() == SerializationTypeKind::Object) {
          render.depth_of_field.enabled =
              read_bool(depth_of_field["enabled"], render.depth_of_field.enabled);
          render.depth_of_field.focus_distance = read_number(
              depth_of_field["focus_distance"],
              render.depth_of_field.focus_distance
          );
          render.depth_of_field.focus_range = read_number(
              depth_of_field["focus_range"],
              render.depth_of_field.focus_range
          );
          render.depth_of_field.max_blur_radius = read_number(
              depth_of_field["max_blur_radius"],
              render.depth_of_field.max_blur_radius
          );
          render.depth_of_field.sample_count = static_cast<int>(read_number(
              depth_of_field["sample_count"],
              static_cast<float>(render.depth_of_field.sample_count)
          ));
        }

        auto god_rays = content["god_rays"];
        if (god_rays.kind() == SerializationTypeKind::Object) {
          render.god_rays.enabled =
              read_bool(god_rays["enabled"], render.god_rays.enabled);
          render.god_rays.intensity =
              read_number(god_rays["intensity"], render.god_rays.intensity);
          render.god_rays.decay =
              read_number(god_rays["decay"], render.god_rays.decay);
          render.god_rays.density =
              read_number(god_rays["density"], render.god_rays.density);
          render.god_rays.weight =
              read_number(god_rays["weight"], render.god_rays.weight);
          render.god_rays.threshold =
              read_number(god_rays["threshold"], render.god_rays.threshold);
          render.god_rays.samples = static_cast<int>(read_number(
              god_rays["samples"],
              static_cast<float>(render.god_rays.samples)
          ));
        }

        auto cas = content["cas"];
        if (cas.kind() == SerializationTypeKind::Object) {
          render.cas.enabled =
              read_bool(cas["enabled"], render.cas.enabled);
          render.cas.sharpness =
              read_number(cas["sharpness"], render.cas.sharpness);
          render.cas.contrast =
              read_number(cas["contrast"], render.cas.contrast);
          render.cas.sharpening_limit =
              read_number(cas["sharpening_limit"], render.cas.sharpening_limit);
        }

        auto taa = content["taa"];
        if (taa.kind() == SerializationTypeKind::Object) {
          render.taa.enabled =
              read_bool(taa["enabled"], render.taa.enabled);
          render.taa.blend_factor =
              read_number(taa["blend_factor"], render.taa.blend_factor);
        }

        auto tonemapping = content["tonemapping"];
        if (tonemapping.kind() == SerializationTypeKind::Object) {
          render.tonemapping.tonemap_operator = static_cast<TonemapOperator>(
              static_cast<int>(read_number(
                  tonemapping["tonemap_operator"],
                  static_cast<float>(static_cast<int>(render.tonemapping.tonemap_operator))
              ))
          );
          render.tonemapping.gamma =
              read_number(tonemapping["gamma"], render.tonemapping.gamma);
          render.tonemapping.bloom_strength =
              read_number(tonemapping["bloom_strength"], render.tonemapping.bloom_strength);
        }

        auto msaa = content["msaa"];

        render.msaa.samples = msaa["samples"].as<int>();
        render.msaa.is_enabled = msaa["is_enabled"].as<bool>();

        config.systems.push_back({
            .name = name,
            .type = system_type,
            .content = render,
        });
      } break;

      case SystemType::Audio: {
        AudioSystemConfig audio;

        audio.backend = sys["content"]["backend"].as<std::string>();
        audio.master_gain = read_number(sys["content"]["master_gain"], 1.0f);

        config.systems.push_back({
            .name = name,
            .type = system_type,
            .content = audio,
        });

        break;
      }

      case SystemType::Terrain: {
        TerrainSystemConfig terrain;

        terrain.default_resolution = static_cast<uint32_t>(
            read_number(sys["content"]["default_resolution"], 1025.0f)
        );
        terrain.clipmap_levels = static_cast<uint32_t>(
            read_number(sys["content"]["clipmap_levels"], 6.0f)
        );
        terrain.tile_world_size =
            read_number(sys["content"]["tile_world_size"], 256.0f);

        config.systems.push_back({
            .name = name,
            .type = system_type,
            .content = terrain,
        });

        break;
      }

      default:
        ASTRA_EXCEPTION("Unkown system type", name)
    }
  }
}

#undef SET_CONFIG

} // namespace astralix
