#include "project-serializer.hpp"

#include "adapters/file/file-stream-reader.hpp"
#include "context-proxy.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace axgen {

namespace {

using astralix::ContextProxy;
using astralix::FileStreamReader;
using astralix::Ref;
using astralix::SerializationContext;
using astralix::SerializationFormat;
using astralix::SerializationTypeKind;

void set_error(std::string *error, std::string message) {
  if (error) {
    *error = std::move(message);
  }
}

std::string to_lower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

std::string format_name(SerializationFormat format) {
  switch (format) {
    case SerializationFormat::Json:
      return "json";
    case SerializationFormat::Yaml:
      return "yaml";
    case SerializationFormat::Toml:
      return "toml";
    case SerializationFormat::Xml:
      return "xml";
  }

  return "unknown";
}

std::vector<SerializationFormat> enabled_formats() {
  std::vector<SerializationFormat> formats;

#if defined(ASTRALIX_SERIALIZATION_ENABLE_JSON)
  formats.push_back(SerializationFormat::Json);
#endif
#if defined(ASTRALIX_SERIALIZATION_ENABLE_YAML)
  formats.push_back(SerializationFormat::Yaml);
#endif
#if defined(ASTRALIX_SERIALIZATION_ENABLE_TOML)
  formats.push_back(SerializationFormat::Toml);
#endif
#if defined(ASTRALIX_SERIALIZATION_ENABLE_XML)
  formats.push_back(SerializationFormat::Xml);
#endif

  return formats;
}

std::optional<SerializationFormat>
serialization_format_from_string(std::string value) {
  value = to_lower(std::move(value));

  if (value == "json") {
    return SerializationFormat::Json;
  }
  if (value == "yaml") {
    return SerializationFormat::Yaml;
  }
  if (value == "toml") {
    return SerializationFormat::Toml;
  }
  if (value == "xml") {
    return SerializationFormat::Xml;
  }

  return std::nullopt;
}

std::optional<std::pair<Ref<SerializationContext>, SerializationFormat>>
load_context(const std::filesystem::path &manifest_path, std::string *error) {
  const auto formats = enabled_formats();
  if (formats.empty()) {
    set_error(error,
              "axgen was built without any serialization formats enabled");
    return std::nullopt;
  }

  std::vector<std::string> failures;
  failures.reserve(formats.size());

  for (const auto format : formats) {
    try {
      FileStreamReader stream(manifest_path);
      stream.read();

      auto ctx = SerializationContext::create(format);
      ctx->from_buffer(stream.get_buffer());
      return std::pair<Ref<SerializationContext>, SerializationFormat>{
          std::move(ctx), format};
    } catch (const std::exception &exception) {
      failures.push_back(format_name(format) + ": " + exception.what());
    }
  }

  std::ostringstream message;
  message << "failed to parse manifest '" << manifest_path.string()
          << "' with the enabled serialization formats";

  for (const auto &failure : failures) {
    message << '\n' << "  - " << failure;
  }

  set_error(error, message.str());
  return std::nullopt;
}

std::optional<ManifestPath> parse_path(ContextProxy ctx, std::string *error) {
  if (ctx.kind() == SerializationTypeKind::Unknown) {
    return std::nullopt;
  }

  if (ctx.kind() != SerializationTypeKind::String) {
    set_error(error, "path must be encoded as a string");
    return std::nullopt;
  }

  const auto full_path = ctx.as<std::string>();
  if (full_path.empty()) {
    set_error(error, "path is empty");
    return std::nullopt;
  }

  ManifestPath parsed_path;

  if (full_path.starts_with('@')) {
    const std::string_view path_view(full_path);
    const auto slash_position = path_view.find('/', 1);

    if (slash_position == std::string_view::npos || slash_position <= 1 ||
        slash_position + 1 >= path_view.size()) {
      set_error(error, "invalid aliased path '" + full_path + "'");
      return std::nullopt;
    }

    const auto alias = path_view.substr(1, slash_position - 1);
    if (alias == "engine") {
      parsed_path.base_directory = BaseDirectory::Engine;
    } else if (alias == "project") {
      parsed_path.base_directory = BaseDirectory::Project;
    } else {
      set_error(error, "unknown path alias '" + std::string(alias) + "'");
      return std::nullopt;
    }

    parsed_path.relative_path =
        std::filesystem::path(full_path.substr(slash_position + 1))
            .lexically_normal()
            .generic_string();
    return parsed_path;
  }

  parsed_path.relative_path =
      std::filesystem::path(full_path).lexically_normal().generic_string();
  return parsed_path;
}

bool has_inline_resource_fields(ContextProxy &asset) {
  return asset["type"].kind() == SerializationTypeKind::String ||
         asset["path"].kind() == SerializationTypeKind::String ||
         asset["vertex"].kind() == SerializationTypeKind::String ||
         asset["fragment"].kind() == SerializationTypeKind::String ||
         asset["compute"].kind() == SerializationTypeKind::String ||
         asset["geometry"].kind() == SerializationTypeKind::String ||
         asset["faces"].kind() == SerializationTypeKind::Array ||
         asset["textures"].kind() == SerializationTypeKind::Object;
}

bool is_asset_binding_entry(ContextProxy &asset) {
  return asset["asset"].kind() == SerializationTypeKind::String;
}

} // namespace

std::optional<ProjectManifest>
ProjectSerializer::deserialize(const std::filesystem::path &manifest_path,
                               std::string *error) {
  const auto normalized_manifest =
      std::filesystem::absolute(manifest_path).lexically_normal();

  if (!std::filesystem::exists(normalized_manifest) ||
      !std::filesystem::is_regular_file(normalized_manifest)) {
    set_error(error, "manifest path '" + normalized_manifest.string() +
                         "' does not exist");
    return std::nullopt;
  }

  auto loaded_context = load_context(normalized_manifest, error);
  if (!loaded_context) {
    return std::nullopt;
  }

  auto &[ctx, detected_format] = *loaded_context;

  ProjectManifest manifest;
  manifest.manifest_path = normalized_manifest;
  manifest.project_root = normalized_manifest.parent_path();
  manifest.serialization_format = detected_format;
  manifest.resources_directory =
      (*ctx)["project"]["resources"]["directory"].as<std::string>();

  const auto declared_project_directory =
      (*ctx)["project"]["directory"].as<std::string>();
  if (!declared_project_directory.empty()) {
    const auto declared_path = std::filesystem::path(declared_project_directory);
    manifest.project_root =
        declared_path.is_absolute()
            ? declared_path.lexically_normal()
            : (normalized_manifest.parent_path() / declared_path)
                  .lexically_normal();
  } else if (!manifest.resources_directory.empty()) {
    const auto direct_resources_root =
        (manifest.project_root / manifest.resources_directory).lexically_normal();
    const auto parent_resources_root =
        (manifest.project_root.parent_path() / manifest.resources_directory)
            .lexically_normal();

    if (!std::filesystem::exists(direct_resources_root) &&
        std::filesystem::exists(parent_resources_root)) {
      manifest.project_root = manifest.project_root.parent_path().lexically_normal();
    }
  }

  const auto declared_format =
      (*ctx)["project"]["serialization"]["format"].as<std::string>();
  if (!declared_format.empty()) {
    auto parsed_format = serialization_format_from_string(declared_format);
    if (!parsed_format) {
      set_error(error, "unsupported project serialization format '" +
                           declared_format + "'");
      return std::nullopt;
    }

    if (*parsed_format != detected_format) {
      set_error(error, "manifest declares serialization format '" +
                           declared_format + "' but axgen parsed it as '" +
                           format_name(detected_format) + "'");
      return std::nullopt;
    }
  }

  auto resources = (*ctx)["resources"];
  const auto resource_count = resources.size();
  manifest.shaders.reserve(resource_count);
  manifest.asset_bindings.reserve(resource_count);

  for (size_t i = 0; i < resource_count; ++i) {
    auto asset = resources[static_cast<int>(i)];

    const bool asset_binding = is_asset_binding_entry(asset);
    const bool inline_resource = has_inline_resource_fields(asset);

    if (asset_binding && inline_resource) {
      set_error(error, "resource entry mixes inline fields with an asset binding");
      return std::nullopt;
    }

    if (asset_binding) {
      const auto id = asset["id"].as<std::string>();
      const auto asset_path = asset["asset"].as<std::string>();
      if (id.empty()) {
        set_error(error, "asset binding id is required");
        return std::nullopt;
      }
      if (asset_path.empty()) {
        set_error(error, "asset binding path is required for '" + id + "'");
        return std::nullopt;
      }

      manifest.asset_bindings.push_back(
          astralix::AssetBindingConfig{.id = id, .asset_path = asset_path}
      );
    }

    if (asset["type"].as<std::string>() != "Shader") {
      continue;
    }

    const auto shader_id = asset["id"].as<std::string>();
    ShaderDescriptorInput descriptor;

    std::string path_error;
    descriptor.vertex_path = parse_path(asset["vertex"], &path_error);
    if (!path_error.empty()) {
      set_error(error, "invalid vertex path for shader '" + shader_id +
                           "': " + path_error);
      return std::nullopt;
    }

    path_error.clear();
    descriptor.fragment_path = parse_path(asset["fragment"], &path_error);
    if (!path_error.empty()) {
      set_error(error, "invalid fragment path for shader '" + shader_id +
                           "': " + path_error);
      return std::nullopt;
    }

    path_error.clear();
    descriptor.geometry_path = parse_path(asset["geometry"], &path_error);
    if (!path_error.empty()) {
      set_error(error, "invalid geometry path for shader '" + shader_id +
                           "': " + path_error);
      return std::nullopt;
    }

    path_error.clear();
    descriptor.compute_path = parse_path(asset["compute"], &path_error);
    if (!path_error.empty()) {
      set_error(error, "invalid compute path for shader '" + shader_id +
                           "': " + path_error);
      return std::nullopt;
    }

    manifest.shaders.push_back(std::move(descriptor));
  }

  return manifest;
}

} // namespace axgen
