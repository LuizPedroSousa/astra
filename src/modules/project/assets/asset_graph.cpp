#include "assets/asset_graph.hpp"

#include "adapters/file/file-stream-reader.hpp"
#include "assert.hpp"
#include "context-proxy.hpp"
#include "log.hpp"
#include "serialization-context.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>

namespace astralix {
namespace {

using ParsedContext = Ref<SerializationContext>;

struct ParsedAssetDefinition {
  AssetKind kind = AssetKind::Texture2D;
  AssetPayload payload = TerrainAssetData{};
  std::vector<ResolvedAssetPath> dependencies;
};

std::string to_lower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
  );
  return value;
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

glm::vec3 read_vec3(
    ContextProxy ctx,
    const glm::vec3 &fallback = glm::vec3(0.0f)
) {
  if (ctx.kind() != SerializationTypeKind::Object) {
    return fallback;
  }

  return glm::vec3(
      read_number(ctx["x"], fallback.x),
      read_number(ctx["y"], fallback.y),
      read_number(ctx["z"], fallback.z)
  );
}

glm::vec4 read_vec4(
    ContextProxy ctx,
    const glm::vec4 &fallback = glm::vec4(0.0f)
) {
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

std::string require_string(ContextProxy ctx, std::string_view message) {
  ASTRA_ENSURE(ctx.kind() != SerializationTypeKind::String, message);
  const auto value = ctx.as<std::string>();
  ASTRA_ENSURE(value.empty(), message);
  return value;
}

std::filesystem::path normalize_relative_path(
    const std::filesystem::path &path,
    std::string_view label
) {
  ASTRA_ENSURE(path.is_absolute(), label, " must be relative");
  const auto normalized = path.lexically_normal();
  const auto generic = normalized.generic_string();
  ASTRA_ENSURE(
      generic.empty() || generic == "." || generic == ".." ||
          generic.starts_with("../"),
      label,
      " escapes its root: ",
      generic
  );
  return normalized;
}

bool is_graph_asset_extension(const std::filesystem::path &path) {
  const auto extension = to_lower(path.extension().string());
  return extension == ".axtexture" || extension == ".axmaterial" ||
         extension == ".axmodel" || extension == ".axfont" ||
         extension == ".axsvg" || extension == ".axaudio" ||
         extension == ".axterrain";
}

bool is_texture_asset_extension(const std::filesystem::path &path) {
  return to_lower(path.extension().string()) == ".axtexture";
}

bool is_material_asset_extension(const std::filesystem::path &path) {
  return to_lower(path.extension().string()) == ".axmaterial";
}

bool is_json_file(const std::filesystem::path &path) {
  return to_lower(path.extension().string()).starts_with(".ax");
}

AssetKind asset_kind_from_path(const std::filesystem::path &path) {
  const auto extension = to_lower(path.extension().string());

  if (extension == ".axtexture") {
    return AssetKind::Texture2D;
  }
  if (extension == ".axmaterial") {
    return AssetKind::Material;
  }
  if (extension == ".axmodel") {
    return AssetKind::Model;
  }
  if (extension == ".axfont") {
    return AssetKind::Font;
  }
  if (extension == ".axsvg") {
    return AssetKind::Svg;
  }
  if (extension == ".axaudio") {
    return AssetKind::AudioClip;
  }
  if (extension == ".axterrain") {
    return AssetKind::TerrainRecipe;
  }

  ASTRA_EXCEPTION("Unsupported asset extension: ", path.extension().string());
}

void require_version_1(ContextProxy version_ctx, std::string_view label) {
  if (version_ctx.kind() == SerializationTypeKind::Unknown) {
    ASTRA_EXCEPTION(label, " must declare version 1");
  }

  ASTRA_ENSURE(
      version_ctx.kind() != SerializationTypeKind::Int,
      label,
      " version must be an integer"
  );
  ASTRA_ENSURE(
      version_ctx.as<int>() != 1,
      "Unsupported ",
      label,
      " version: ",
      version_ctx.as<int>()
  );
}

ParsedContext load_json_context(const std::filesystem::path &path) {
  ASTRA_ENSURE(!std::filesystem::exists(path), "Asset file not found: ", path);
  ASTRA_ENSURE(!std::filesystem::is_regular_file(path), "Asset path is not a file: ", path);
  ASTRA_ENSURE(!is_json_file(path), "Asset file must use an .ax* extension: ", path);

  FileStreamReader stream(path);
  stream.read();

  return SerializationContext::create(
      SerializationFormat::Json,
      stream.get_buffer()
  );
}

std::optional<std::string> read_optional_asset_key(
    std::vector<ResolvedAssetPath> &dependencies,
    const std::function<ResolvedAssetPath(std::string_view)> &resolver,
    ContextProxy ctx,
    std::string_view expected_extension,
    const std::function<std::string(const ResolvedAssetPath &)> &key_builder
) {
  if (ctx.kind() != SerializationTypeKind::String) {
    return std::nullopt;
  }

  auto resolved = resolver(ctx.as<std::string>());
  ASTRA_ENSURE(
      to_lower(resolved.relative_path.extension().string()) !=
          std::string(expected_extension),
      "Expected asset reference with extension ",
      expected_extension,
      ", got ",
      format_asset_reference(resolved)
  );

  dependencies.push_back(resolved);
  return key_builder(resolved);
}

} // namespace

AssetGraph::AssetGraph(AssetGraphConfig config) : m_config(std::move(config)) {}

void AssetGraph::load_root_assets(std::span<const AssetBindingConfig> roots) {
  m_records.clear();
  m_topological_order.clear();
  m_record_index_by_key.clear();
  m_public_record_index_by_id.clear();
  m_public_id_by_key.clear();
  m_asset_key_by_public_id.clear();

  for (const auto &root : roots) {
    ASTRA_ENSURE(root.id.empty(), "Asset binding id is required");
    ASTRA_ENSURE(
        root.asset_path.empty(),
        "Asset binding path is required for ",
        root.id
    );

    const auto resolved = resolve_reference(std::nullopt, root.asset_path);
    const auto asset_key = to_asset_key(resolved);

    auto [id_it, inserted_public_id] =
        m_asset_key_by_public_id.emplace(root.id, asset_key);
    ASTRA_ENSURE(
        !inserted_public_id && id_it->second != asset_key,
        "Duplicate asset binding id: ",
        root.id
    );

    auto [key_it, inserted_asset_key] =
        m_public_id_by_key.emplace(asset_key, root.id);
    ASTRA_ENSURE(
        !inserted_asset_key && key_it->second != root.id,
        "Multiple public IDs target the same asset: ",
        format_asset_reference(resolved)
    );

    std::vector<std::string> stack;
    load_asset_recursive(resolved, stack);
  }

  finalize_records();
}

const AssetRecord *AssetGraph::find_by_public_id(std::string_view id) const {
  const auto it = m_public_record_index_by_id.find(std::string(id));
  if (it == m_public_record_index_by_id.end()) {
    return nullptr;
  }

  return &m_records[it->second];
}

const AssetRecord *
AssetGraph::find_by_asset_path(const std::filesystem::path &path) const {
  const auto normalized = path.lexically_normal();
  const auto is_absolute = normalized.is_absolute();

  for (const auto &record : m_records) {
    if (is_absolute && record.absolute_path == normalized) {
      return &record;
    }

    if (!is_absolute && record.asset_path.relative_path == normalized) {
      return &record;
    }
  }

  return nullptr;
}

const AssetRecord *
AssetGraph::find_by_asset_key(const std::string &key) const {
  auto it = m_record_index_by_key.find(key);
  if (it == m_record_index_by_key.end()) {
    return nullptr;
  }

  return &m_records[it->second];
}

std::span<const AssetRecord> AssetGraph::records() const { return m_records; }

std::span<const AssetRecord *const> AssetGraph::topological_order() const {
  return m_topological_order;
}

ResolvedAssetPath AssetGraph::resolve_reference(
    const std::optional<ResolvedAssetPath> &owner_asset_path,
    std::string_view reference
) const {
  ASTRA_ENSURE(reference.empty(), "Asset reference is empty");

  std::string value(reference);
  ResolvedAssetPath resolved;

  if (value.starts_with('@')) {
    const std::string_view value_view(value);
    const auto slash_position = value_view.find('/', 1);
    ASTRA_ENSURE(
        slash_position == std::string_view::npos || slash_position <= 1 ||
            slash_position + 1 >= value_view.size(),
        "Invalid aliased asset path: ",
        value
    );

    const auto alias = value_view.substr(1, slash_position - 1);
    ASTRA_ENSURE(
        alias != "engine" && alias != "project",
        "Unknown asset alias: ",
        std::string(alias)
    );

    resolved.base_directory =
        alias == "engine" ? BaseDirectory::Engine : BaseDirectory::Project;
    resolved.relative_path = normalize_relative_path(
        std::filesystem::path(value.substr(slash_position + 1)),
        "Aliased asset path"
    );
    return resolved;
  }

  if (owner_asset_path.has_value()) {
    resolved.base_directory = owner_asset_path->base_directory;
    resolved.relative_path = normalize_relative_path(
        owner_asset_path->relative_path.parent_path() / value,
        "Nested asset path"
    );
    return resolved;
  }

  resolved.base_directory = BaseDirectory::Project;
  resolved.relative_path =
      normalize_relative_path(std::filesystem::path(value), "Asset path");
  return resolved;
}

std::filesystem::path
AssetGraph::to_absolute_path(const ResolvedAssetPath &path) const {
  switch (path.base_directory) {
  case BaseDirectory::Engine:
    return (m_config.engine_assets_root / path.relative_path).lexically_normal();
  case BaseDirectory::Project:
    return (m_config.project_resources_root / path.relative_path)
        .lexically_normal();
  default:
    ASTRA_EXCEPTION("Unsupported asset base directory");
  }
}

std::string AssetGraph::to_asset_key(const ResolvedAssetPath &path) const {
  const auto prefix =
      path.base_directory == BaseDirectory::Engine ? "engine/" : "project/";
  return prefix + path.relative_path.lexically_normal().generic_string();
}

void AssetGraph::load_asset_recursive(
    const ResolvedAssetPath &asset_path,
    std::vector<std::string> &stack
) {
  const auto asset_key = to_asset_key(asset_path);

  if (m_record_index_by_key.contains(asset_key)) {
    return;
  }

  if (std::find(stack.begin(), stack.end(), asset_key) != stack.end()) {
    std::ostringstream cycle;
    for (const auto &step : stack) {
      if (!cycle.str().empty()) {
        cycle << " -> ";
      }
      cycle << step;
    }
    cycle << " -> " << asset_key;
    ASTRA_EXCEPTION("Asset dependency cycle detected: ", cycle.str());
  }

  stack.push_back(asset_key);

  const auto absolute_path = to_absolute_path(asset_path);
  auto ctx = load_json_context(absolute_path);
  ASTRA_ENSURE(
      ctx->kind() != SerializationTypeKind::Object,
      "Asset root must be an object: ",
      absolute_path
  );

  const auto kind = asset_kind_from_path(asset_path.relative_path);
  auto field = [&](const char *key) { return (*ctx)[key]; };

  auto resolve_from_owner = [&](std::string_view reference) {
    return resolve_reference(asset_path, reference);
  };
  auto key_builder = [&](const ResolvedAssetPath &resolved) {
    return to_asset_key(resolved);
  };

  ParsedAssetDefinition parsed;
  parsed.kind = kind;

  switch (kind) {
  case AssetKind::Texture2D: {
    require_version_1(field("version"), ".axtexture");

    TextureAssetData data;
    data.source_path =
        resolve_from_owner(require_string(field("source"), ".axtexture source is required"));
    ASTRA_ENSURE(
        is_graph_asset_extension(data.source_path.relative_path),
        ".axtexture source must reference a raw payload, got ",
        format_asset_reference(data.source_path)
    );
    data.flip = read_bool(field("flip"), false);

    auto parameters = field("parameters");
    if (parameters.kind() == SerializationTypeKind::Object) {
      for (const auto &key : parameters.object_keys()) {
        auto value_ctx = parameters[key];
        ASTRA_ENSURE(
            value_ctx.kind() != SerializationTypeKind::String,
            ".axtexture parameter values must be strings"
        );
        data.parameters.push_back(
            TextureParameterEntry{.key = key, .value = value_ctx.as<std::string>()}
        );
      }
    }

    parsed.payload = std::move(data);
    break;
  }
  case AssetKind::Material: {
    require_version_1(field("version"), ".axmaterial");

    MaterialAssetData data;
    auto textures = field("textures");
    if (textures.kind() == SerializationTypeKind::Object) {
      data.base_color_asset_key = read_optional_asset_key(
          parsed.dependencies,
          resolve_from_owner,
          textures["base_color"],
          ".axtexture",
          key_builder
      );
      data.normal_asset_key = read_optional_asset_key(
          parsed.dependencies,
          resolve_from_owner,
          textures["normal"],
          ".axtexture",
          key_builder
      );
      data.metallic_asset_key = read_optional_asset_key(
          parsed.dependencies,
          resolve_from_owner,
          textures["metallic"],
          ".axtexture",
          key_builder
      );
      data.roughness_asset_key = read_optional_asset_key(
          parsed.dependencies,
          resolve_from_owner,
          textures["roughness"],
          ".axtexture",
          key_builder
      );
      data.metallic_roughness_asset_key = read_optional_asset_key(
          parsed.dependencies,
          resolve_from_owner,
          textures["metallic_roughness"],
          ".axtexture",
          key_builder
      );
      data.occlusion_asset_key = read_optional_asset_key(
          parsed.dependencies,
          resolve_from_owner,
          textures["occlusion"],
          ".axtexture",
          key_builder
      );
      data.emissive_asset_key = read_optional_asset_key(
          parsed.dependencies,
          resolve_from_owner,
          textures["emissive"],
          ".axtexture",
          key_builder
      );
      data.displacement_asset_key = read_optional_asset_key(
          parsed.dependencies,
          resolve_from_owner,
          textures["displacement"],
          ".axtexture",
          key_builder
      );
    }

    data.base_color_factor =
        read_vec4(field("base_color_factor"), glm::vec4(1.0f));
    data.emissive_factor =
        read_vec3(field("emissive_factor"), glm::vec3(0.0f));
    data.metallic_factor = read_number(field("metallic_factor"), 1.0f);
    data.roughness_factor = read_number(field("roughness_factor"), 1.0f);
    data.occlusion_strength = read_number(field("occlusion_strength"), 1.0f);
    data.normal_scale = read_number(field("normal_scale"), 1.0f);
    data.height_scale = read_number(field("height_scale"), 0.0f);
    data.bloom_intensity = read_number(field("bloom_intensity"), 0.0f);
    data.alpha_mask = read_bool(field("alpha_mask"), false);
    data.alpha_blend = read_bool(field("alpha_blend"), false);
    data.alpha_cutoff = read_number(field("alpha_cutoff"), 0.5f);
    data.double_sided = read_bool(field("double_sided"), false);

    if (const auto alpha_mode = read_optional_string(field("alpha_mode"));
        alpha_mode.has_value()) {
      const auto normalized = astralix::to_lower(*alpha_mode);
      ASTRA_ENSURE(
          normalized != "opaque" && normalized != "mask" &&
              normalized != "blend",
          "Unsupported .axmaterial alpha_mode: ",
          *alpha_mode
      );
      data.alpha_mask = normalized == "mask";
      data.alpha_blend = normalized == "blend";
    }

    parsed.payload = std::move(data);
    break;
  }
  case AssetKind::Model: {
    require_version_1(field("version"), ".axmodel");

    ModelAssetData data;
    data.source_path =
        resolve_from_owner(require_string(field("source"), ".axmodel source is required"));
    ASTRA_ENSURE(
        is_graph_asset_extension(data.source_path.relative_path),
        ".axmodel source must reference a raw payload, got ",
        format_asset_reference(data.source_path)
    );

    auto import = field("import");
    if (import.kind() == SerializationTypeKind::Object) {
      data.import.triangulate =
          read_bool(import["triangulate"], data.import.triangulate);
      data.import.flip_uvs =
          read_bool(import["flip_uvs"], data.import.flip_uvs);
      data.import.generate_normals =
          read_bool(import["generate_normals"], data.import.generate_normals);
      data.import.calculate_tangents = read_bool(
          import["calculate_tangents"],
          data.import.calculate_tangents
      );
      data.import.pre_transform_vertices = read_bool(
          import["pre_transform_vertices"],
          data.import.pre_transform_vertices
      );
    }

    auto materials = field("materials");
    if (materials.kind() == SerializationTypeKind::Array) {
      for (size_t index = 0; index < materials.size(); ++index) {
        auto material_asset_key = read_optional_asset_key(
            parsed.dependencies,
            resolve_from_owner,
            materials[static_cast<int>(index)],
            ".axmaterial",
            key_builder
        );
        ASTRA_ENSURE(
            !material_asset_key.has_value(),
            ".axmodel materials entries must be strings"
        );
        data.material_asset_keys.push_back(*material_asset_key);
      }
    }

    parsed.payload = std::move(data);
    break;
  }
  case AssetKind::Font:
  case AssetKind::Svg:
  case AssetKind::AudioClip: {
    require_version_1(
        field("version"),
        kind == AssetKind::Font
            ? ".axfont"
            : (kind == AssetKind::Svg ? ".axsvg" : ".axaudio")
    );

    SingleSourceAssetData data;
    data.source_path = resolve_from_owner(
        require_string(field("source"), "Single-source asset source is required")
    );
    ASTRA_ENSURE(
        is_graph_asset_extension(data.source_path.relative_path),
        "Single-source asset source must reference a raw payload, got ",
        format_asset_reference(data.source_path)
    );
    parsed.payload = std::move(data);
    break;
  }
  case AssetKind::TerrainRecipe: {
    auto terrain_root = field("terrain");
    const bool wrapped =
        terrain_root.kind() == SerializationTypeKind::Object;
    auto terrain_field = [&](const char *key) {
      return wrapped ? terrain_root[key] : field(key);
    };

    if (terrain_field("version").kind() != SerializationTypeKind::Unknown) {
      ASTRA_ENSURE(
          terrain_field("version").kind() != SerializationTypeKind::Int,
          ".axterrain version must be an integer"
      );
      ASTRA_ENSURE(
          terrain_field("version").as<int>() != 1,
          "Unsupported .axterrain version: ",
          terrain_field("version").as<int>()
      );
    }

    TerrainAssetData data;
    if (terrain_field("version").kind() == SerializationTypeKind::Int) {
      data.version = static_cast<uint32_t>(terrain_field("version").as<int>());
    }
    parsed.payload = std::move(data);
    break;
  }
  }

  std::unordered_set<std::string> seen_dependencies;
  std::vector<AssetDependency> dependencies;
  dependencies.reserve(parsed.dependencies.size());

  for (const auto &dependency_path : parsed.dependencies) {
    ASTRA_ENSURE(
        !is_graph_asset_extension(dependency_path.relative_path),
        "Nested asset dependency must point to another .ax* file: ",
        format_asset_reference(dependency_path)
    );

    const auto dependency_key = to_asset_key(dependency_path);
    if (!seen_dependencies.insert(dependency_key).second) {
      continue;
    }

    load_asset_recursive(dependency_path, stack);
    dependencies.push_back(AssetDependency{
        .asset_path = dependency_path,
        .asset_key = dependency_key,
        .descriptor_id = {},
    });
  }

  stack.pop_back();

  m_record_index_by_key.emplace(asset_key, m_records.size());
  m_records.push_back(AssetRecord{
      .kind = kind,
      .asset_path = asset_path,
      .asset_key = asset_key,
      .absolute_path = absolute_path,
      .descriptor_id = {},
      .public_id = std::nullopt,
      .dependencies = std::move(dependencies),
      .payload = std::move(parsed.payload),
  });
}

bool AssetGraph::reload_asset(const std::string &asset_key) {
  auto index_it = m_record_index_by_key.find(asset_key);
  if (index_it == m_record_index_by_key.end()) {
    LOG_WARN("AssetGraph: reload_asset called for unknown key:", asset_key);
    return false;
  }

  auto &record = m_records[index_it->second];
  const auto absolute_path = to_absolute_path(record.asset_path);

  std::error_code error_code;
  if (!std::filesystem::exists(absolute_path, error_code)) {
    LOG_ERROR("AssetGraph: asset file not found:", absolute_path.string());
    return false;
  }

  LOG_INFO("AssetGraph: re-parsing", asset_key, "from", absolute_path.string());

  auto ctx = load_json_context(absolute_path);
  if (ctx->kind() != SerializationTypeKind::Object) {
    LOG_ERROR("AssetGraph: asset root is not an object:", absolute_path.string());
    return false;
  }

  const auto kind = asset_kind_from_path(record.asset_path.relative_path);
  auto field = [&](const char *key) { return (*ctx)[key]; };

  auto resolve_from_owner = [&](std::string_view reference) {
    return resolve_reference(record.asset_path, reference);
  };
  auto key_builder = [&](const ResolvedAssetPath &resolved) {
    return to_asset_key(resolved);
  };

  ParsedAssetDefinition parsed;
  parsed.kind = kind;

  switch (kind) {
  case AssetKind::Texture2D: {
    TextureAssetData data;
    data.source_path = resolve_from_owner(
        require_string(field("source"), ".axtexture source is required")
    );
    data.flip = read_bool(field("flip"), false);

    auto parameters = field("parameters");
    if (parameters.kind() == SerializationTypeKind::Object) {
      for (const auto &key : parameters.object_keys()) {
        auto value_ctx = parameters[key];
        if (value_ctx.kind() == SerializationTypeKind::String) {
          data.parameters.push_back(
              TextureParameterEntry{.key = key, .value = value_ctx.as<std::string>()}
          );
        }
      }
    }

    parsed.payload = std::move(data);
    break;
  }
  case AssetKind::Material: {
    MaterialAssetData data;
    auto textures = field("textures");
    if (textures.kind() == SerializationTypeKind::Object) {
      data.base_color_asset_key = read_optional_asset_key(
          parsed.dependencies, resolve_from_owner, textures["base_color"],
          ".axtexture", key_builder
      );
      data.normal_asset_key = read_optional_asset_key(
          parsed.dependencies, resolve_from_owner, textures["normal"],
          ".axtexture", key_builder
      );
      data.metallic_asset_key = read_optional_asset_key(
          parsed.dependencies, resolve_from_owner, textures["metallic"],
          ".axtexture", key_builder
      );
      data.roughness_asset_key = read_optional_asset_key(
          parsed.dependencies, resolve_from_owner, textures["roughness"],
          ".axtexture", key_builder
      );
      data.metallic_roughness_asset_key = read_optional_asset_key(
          parsed.dependencies, resolve_from_owner,
          textures["metallic_roughness"], ".axtexture", key_builder
      );
      data.occlusion_asset_key = read_optional_asset_key(
          parsed.dependencies, resolve_from_owner, textures["occlusion"],
          ".axtexture", key_builder
      );
      data.emissive_asset_key = read_optional_asset_key(
          parsed.dependencies, resolve_from_owner, textures["emissive"],
          ".axtexture", key_builder
      );
      data.displacement_asset_key = read_optional_asset_key(
          parsed.dependencies, resolve_from_owner, textures["displacement"],
          ".axtexture", key_builder
      );
    }

    data.base_color_factor =
        read_vec4(field("base_color_factor"), glm::vec4(1.0f));
    data.emissive_factor =
        read_vec3(field("emissive_factor"), glm::vec3(0.0f));
    data.metallic_factor = read_number(field("metallic_factor"), 1.0f);
    data.roughness_factor = read_number(field("roughness_factor"), 1.0f);
    data.occlusion_strength = read_number(field("occlusion_strength"), 1.0f);
    data.normal_scale = read_number(field("normal_scale"), 1.0f);
    data.height_scale = read_number(field("height_scale"), 0.0f);
    data.bloom_intensity = read_number(field("bloom_intensity"), 0.0f);
    data.alpha_mask = read_bool(field("alpha_mask"), false);
    data.alpha_blend = read_bool(field("alpha_blend"), false);
    data.alpha_cutoff = read_number(field("alpha_cutoff"), 0.5f);
    data.double_sided = read_bool(field("double_sided"), false);

    if (const auto alpha_mode = read_optional_string(field("alpha_mode"));
        alpha_mode.has_value()) {
      const auto normalized = astralix::to_lower(*alpha_mode);
      data.alpha_mask = normalized == "mask";
      data.alpha_blend = normalized == "blend";
    }

    parsed.payload = std::move(data);
    break;
  }
  case AssetKind::Model: {
    ModelAssetData data;
    data.source_path = resolve_from_owner(
        require_string(field("source"), ".axmodel source is required")
    );

    auto import_field = field("import");
    if (import_field.kind() == SerializationTypeKind::Object) {
      data.import.triangulate =
          read_bool(import_field["triangulate"], data.import.triangulate);
      data.import.flip_uvs =
          read_bool(import_field["flip_uvs"], data.import.flip_uvs);
      data.import.generate_normals =
          read_bool(import_field["generate_normals"], data.import.generate_normals);
      data.import.calculate_tangents = read_bool(
          import_field["calculate_tangents"], data.import.calculate_tangents
      );
      data.import.pre_transform_vertices = read_bool(
          import_field["pre_transform_vertices"],
          data.import.pre_transform_vertices
      );
    }

    auto materials = field("materials");
    if (materials.kind() == SerializationTypeKind::Array) {
      for (size_t index = 0; index < materials.size(); ++index) {
        auto material_asset_key = read_optional_asset_key(
            parsed.dependencies, resolve_from_owner,
            materials[static_cast<int>(index)], ".axmaterial", key_builder
        );
        if (material_asset_key.has_value()) {
          data.material_asset_keys.push_back(*material_asset_key);
        }
      }
    }

    parsed.payload = std::move(data);
    break;
  }
  case AssetKind::Font:
  case AssetKind::Svg:
  case AssetKind::AudioClip: {
    SingleSourceAssetData data;
    data.source_path = resolve_from_owner(
        require_string(field("source"), "Single-source asset source is required")
    );
    parsed.payload = std::move(data);
    break;
  }
  case AssetKind::TerrainRecipe: {
    auto terrain_root = field("terrain");
    const bool wrapped =
        terrain_root.kind() == SerializationTypeKind::Object;
    auto terrain_field = [&](const char *key) {
      return wrapped ? terrain_root[key] : field(key);
    };

    TerrainAssetData data;
    if (terrain_field("version").kind() == SerializationTypeKind::Int) {
      data.version = static_cast<uint32_t>(terrain_field("version").as<int>());
    }
    parsed.payload = std::move(data);
    break;
  }
  }

  std::vector<AssetDependency> dependencies;
  dependencies.reserve(parsed.dependencies.size());
  for (const auto &dependency_path : parsed.dependencies) {
    const auto dependency_key = to_asset_key(dependency_path);
    const auto dep_index_it = m_record_index_by_key.find(dependency_key);
    ResourceDescriptorID dep_descriptor_id;
    if (dep_index_it != m_record_index_by_key.end()) {
      dep_descriptor_id = m_records[dep_index_it->second].descriptor_id;
    } else {
      dep_descriptor_id = "asset::" + dependency_key;
    }

    dependencies.push_back(AssetDependency{
        .asset_path = dependency_path,
        .asset_key = dependency_key,
        .descriptor_id = dep_descriptor_id,
    });
  }

  record.payload = std::move(parsed.payload);
  record.dependencies = std::move(dependencies);

  LOG_INFO(
      "AssetGraph: reloaded", asset_key,
      "with", record.dependencies.size(), "dependencies"
  );

  return true;
}

void AssetGraph::finalize_records() {
  m_topological_order.clear();
  m_public_record_index_by_id.clear();
  m_topological_order.reserve(m_records.size());

  for (size_t index = 0; index < m_records.size(); ++index) {
    auto &record = m_records[index];
    auto public_id_it = m_public_id_by_key.find(record.asset_key);
    if (public_id_it != m_public_id_by_key.end()) {
      record.public_id = public_id_it->second;
      record.descriptor_id = public_id_it->second;
      m_public_record_index_by_id.emplace(public_id_it->second, index);
    } else {
      record.descriptor_id = "asset::" + record.asset_key;
    }
  }

  for (auto &record : m_records) {
    for (auto &dependency : record.dependencies) {
      const auto it = m_record_index_by_key.find(dependency.asset_key);
      ASTRA_ENSURE(
          it == m_record_index_by_key.end(),
          "Missing asset dependency record for ",
          dependency.asset_key
      );
      dependency.descriptor_id = m_records[it->second].descriptor_id;
    }

    m_topological_order.push_back(&record);
  }
}

} // namespace astralix
